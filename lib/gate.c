#include <inc/lib.h>
#include <inc/gate.h>
#include <inc/setjmp.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/atomic.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
#include <inc/error.h>
#include <inc/mlt.h>

struct gate_call_args {
    struct cobj_ref return_gate;
    struct cobj_ref arg;
};

struct gate_entry_stack_info {
    struct cobj_ref obj;
    void *va;
};

enum {
    gate_debug = 0,
};

// Copy the writable pieces of the address space
enum {
    gate_cow_label_ents = 64,
    gate_cow_as_ents = 16,
};

static void
gate_cow_compute_label(struct ulabel *cur_label, struct ulabel *obj_label)
{
    for (uint32_t j = 0; j < cur_label->ul_nent; j++) {
	uint64_t h = LB_HANDLE(cur_label->ul_ent[j]);
	level_t obj_level = label_get_level(obj_label, h);
	level_t cur_level = label_get_level(cur_label, h);
	if (cur_level == LB_LEVEL_STAR)
	    continue;
	if (obj_level == LB_LEVEL_STAR || obj_level < cur_level)
	    assert(0 == label_set_level(obj_label, h, cur_level, 0));
    }
}

static void
gate_cow(void)
{
    uint64_t cur_ents[gate_cow_label_ents];
    uint64_t obj_ents[gate_cow_label_ents];

    struct ulabel cur_label =
	{ .ul_size = gate_cow_label_ents, .ul_ent = &cur_ents[0] };
    struct ulabel obj_label =
	{ .ul_size = gate_cow_label_ents, .ul_ent = &obj_ents[0] };

    int r = thread_get_label(&cur_label);
    if (r < 0)
	panic("gate_cow: thread_get_label: %s", e2s(r));

    struct cobj_ref cur_as;
    r = sys_thread_get_as(&cur_as);
    if (r < 0)
	panic("gate_cow: sys_thread_get_as: %s", e2s(r));

    r = sys_obj_get_label(cur_as, &obj_label);
    if (r < 0)
	panic("gate_cow: cannot get as label: %s", e2s(r));

    // if we can write to the address space, that's "good enough"
    r = label_compare(&cur_label, &obj_label, label_leq_starlo);
    if (r == 0) {
	if (gate_debug)
	    printf("gate_cow: no need to cow\n");
	return;
    }

    start_env_t *start_env_ro = (start_env_t *) USTARTENVRO;
    int64_t id = container_find(start_env_ro->container, kobj_mlt, "dynamic taint");
    if (id < 0)
	panic("gate_cow: cannot find the MLT: %s", e2s(id));

    r = sys_obj_get_label(COBJ(start_env_ro->container, start_env_ro->container), &obj_label);
    if (r < 0)
	panic("gate_cow: cannot get parent container label: %s", e2s(r));

    gate_cow_compute_label(&cur_label, &obj_label);

    struct cobj_ref mlt = COBJ(start_env_ro->container, id);
    char buf[MLT_BUF_SIZE];
    r = sys_mlt_put(mlt, &obj_label, &buf[0]);
    if (r < 0)
	panic("gate_cow: cannot store garbage in MLT: %s", e2s(r));

    uint64_t mlt_ct;
    r = sys_mlt_get(mlt, &buf[0], &mlt_ct);
    if (r < 0)
	panic("gate_cow: cannot get MLT container: %s", e2s(r));

    struct u_segment_mapping uas_ents[gate_cow_as_ents];
    struct u_address_space uas =
	{ .size = gate_cow_as_ents, .ents = &uas_ents[0] };
    r = sys_as_get(cur_as, &uas);
    if (r < 0)
	panic("gate_cow: sys_as_get: %s", e2s(r));

    for (uint32_t i = 0; i < uas.nent; i++) {
	if (!(uas.ents[i].flags & SEGMAP_WRITE))
	    continue;

	r = sys_obj_get_label(uas.ents[i].segment, &obj_label);
	if (r < 0)
	    panic("gate_cow: cannot get label: %s", e2s(r));

	r = label_compare(&cur_label, &obj_label, label_leq_starlo);
	if (r == 0)
	    continue;

	if (gate_debug)
	    cprintf("gate_cow: trying to copy segment %ld.%ld\n",
		    uas.ents[i].segment.container,
		    uas.ents[i].segment.object);

	gate_cow_compute_label(&cur_label, &obj_label);

	id = sys_segment_copy(uas.ents[i].segment, mlt_ct,
			      &obj_label, "gate cow");
	if (id < 0)
	    panic("gate_cow: cannot copy segment: %s", e2s(id));

	uas.ents[i].segment = COBJ(mlt_ct, id);
    }

    r = sys_obj_get_label(cur_as, &obj_label);
    if (r < 0)
	panic("gate_cow: cannot get as label again: %s", e2s(r));

    gate_cow_compute_label(&cur_label, &obj_label);

    id = sys_as_create(mlt_ct, &obj_label, "gate cow as");
    if (id < 0)
	panic("gate_cow: cannot create new as: %s", e2s(id));

    struct cobj_ref new_as = COBJ(mlt_ct, id);
    r = sys_as_set(new_as, &uas);
    if (r < 0)
	panic("gate_cow: cannot populate new as: %s", e2s(r));

    r = sys_thread_set_as(new_as);
    if (r < 0)
	panic("gate_cow: cannot switch to new as: %s", e2s(r));

    if (gate_debug)
	cprintf("gate_cow: new as: %lu.%lu\n", new_as.container, new_as.object);

    struct ulabel *l_seg = segment_get_default_label();
    if (l_seg) {
	gate_cow_compute_label(&cur_label, l_seg);
	segment_set_default_label(l_seg);
    }
}

// Compute the appropriate gate entry label.
static int
gate_compute_max_label(struct ulabel *g)
{
    struct ulabel *cur = label_get_current();
    if (cur == 0) {
	printf("gate_compute_return_label: cannot get current label");
	return -E_INVAL;
    }

    int r = 0;
    for (uint32_t i = 0; i < cur->ul_nent; i++) {
	uint64_t h = LB_HANDLE(cur->ul_ent[i]);
	level_t cur_lv = LB_LEVEL(cur->ul_ent[i]);
	if (cur_lv == LB_LEVEL_STAR)
	    continue;

	level_t g_lv = label_get_level(g, h);
	if (cur_lv > g_lv) {
	    r = label_set_level(g, h, cur_lv, 0);
	    if (r < 0)
		goto out;
	}
    }

    if (cur->ul_default > g->ul_default && cur->ul_default != LB_LEVEL_STAR)
	g->ul_default = cur->ul_default;

out:
    label_free(cur);
    return r;
}

static void __attribute__((noreturn))
gate_return_entrystack(struct u_gate_entry *ug,
		       struct cobj_ref *return_gate_p,
		       struct cobj_ref *arg_p,
		       struct gate_entry_stack_info *stackinfp)
{
    struct gate_entry_stack_info stackinf = *stackinfp;
    struct cobj_ref return_gate = *return_gate_p;
    struct cobj_ref arg = *arg_p;

    segment_unmap(stackinf.va);
    sys_obj_unref(stackinf.obj);

    // Compute the return label -- try to grant as little as possible
    uint64_t nents = 64;
    uint64_t gate_label_ents[nents];
    struct ulabel gate_label = { .ul_size = nents, .ul_ent = &gate_label_ents[0] };

    int r = sys_gate_send_label(return_gate, &gate_label);
    if (r < 0)
	panic("gate_return_entrystack: getting send label: %s", e2s(r));

    r = gate_compute_max_label(&gate_label);
    if (r < 0)
	panic("gate_return_entrystack: computing return label: %s", e2s(r));

    r = sys_obj_unref(COBJ(ug->container, thread_id()));
    if (r < 0)
	panic("gate_return_entrystack: unref: %s", e2s(r));

    if (gate_debug)
	printf("gate_return_entrystack: return label %s\n",
	       label_to_string(&gate_label));

    r = sys_gate_enter(return_gate, &gate_label, arg.container, arg.object);
    panic("gate_return_entrystack: gate_enter: label %s: %s",
	  label_to_string(&gate_label), e2s(r));
}

static void __attribute__((noreturn))
gate_entry_newstack(struct u_gate_entry *ug,
		    struct gate_entry_stack_info *stackinfp)
{
    struct gate_entry_stack_info stackinf = *stackinfp;

    struct gate_call_args *call_args = ug->stackbase;
    struct cobj_ref arg = call_args->arg;
    struct cobj_ref return_gate = call_args->return_gate;

    ug->func(ug->func_arg, &arg);

    stack_switch((uint64_t) ug,
		 (uint64_t) &return_gate,
		 (uint64_t) &arg,
		 (uint64_t) &stackinf,
		 ug->stackbase + PGSIZE,
		 &gate_return_entrystack);
}

static void __attribute__((noreturn))
gate_entry(struct u_gate_entry *ug)
{
    if (gate_debug) {
	struct ulabel *l = label_get_current();
	if (l == 0) {
	    printf("gate_entry: cannot get entry label\n");
	} else {
	    printf("gate_entry: entry label %s\n", label_to_string(l));
	    label_free(l);
	}
    }

    int r = sys_thread_addref(ug->container);
    if (r < 0)
	panic("gate_entry: thread_addref ct=%ld: %s", ug->container, e2s(r));

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(ug->gate, &name[0]);
    if (r < 0)
	panic("gate_entry_locked: sys_obj_get_name: %s", e2s(r));

    char sname[KOBJ_NAME_LEN];
    snprintf(&sname[0], KOBJ_NAME_LEN, "gate stack for %s", name);

    int stackpages = 2;
    struct gate_entry_stack_info stackinf;
    stackinf.va = 0;
    r = segment_alloc(ug->container, stackpages * PGSIZE,
		      &stackinf.obj, &stackinf.va, 0, &sname[0]);
    if (r < 0)
	panic("gate_entry_locked: cannot allocate new stack: %s", e2s(r));

    stack_switch((uint64_t) ug, (uint64_t) &stackinf, 0, 0,
		 stackinf.va + stackpages * PGSIZE,
		 &gate_entry_newstack);
}

int
gate_create(struct u_gate_entry *ug,
	    uint64_t gate_container,
	    uint64_t entry_container,
	    void (*func) (void*, struct cobj_ref*), void *func_arg,
	    const char *name, struct ulabel *l_send)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    int r = sys_segment_resize(tseg, 1);
    if (r < 0)
	return r;

    ug->stackbase = 0;
    r = segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &ug->stackbase, 0);
    if (r < 0)
	return r;

    ug->container = entry_container;
    ug->func = func;
    ug->func_arg = func_arg;

    // XXX this should probably be more constrainted, or argument-specified
    struct ulabel l_recv = { .ul_nent = 0, .ul_default = 3 };

    if (l_send == 0)
	l_send = label_get_current();
    if (l_send == 0)
	return -E_NO_MEM;

    struct thread_entry te = {
	.te_entry = &gate_entry,
	.te_stack = ug->stackbase + PGSIZE,
	.te_arg = (uint64_t) ug,
    };

    r = sys_thread_get_as(&te.te_as);
    if (r < 0)
	goto out;

    int64_t gate_id = sys_gate_create(gate_container, &te,
				      &l_recv, l_send, name);
    if (gate_id < 0) {
	r = gate_id;
	goto out;
    }

    ug->gate = COBJ(gate_container, gate_id);
    return 0;

out:
    segment_unmap(ug->stackbase);
    return r;
}

struct gate_return {
    atomic_t return_count;
    int *rvalp;
    struct cobj_ref *argp;
    struct jmp_buf *return_jmpbuf;
    int64_t return_handle;
};

static void __attribute__((noreturn))
gate_call_return(struct gate_return *gr, struct cobj_ref arg)
{
    if (gate_debug)
	cprintf("gate_call_return: hello world\n");

    gate_cow();

    if (gate_debug)
	cprintf("gate_call_return: after gate_cow\n");

    if (atomic_compare_exchange(&gr->return_count, 0, 1) != 0)
	panic("gate_call_return: multiple return");

    struct ulabel *l = label_get_current();
    assert(l);

    assert(0 == label_set_level(l, gr->return_handle, l->ul_default, 1));

    if (gate_debug)
	cprintf("gate_call_return: switching label to %s\n",
		label_to_string(l));

    assert(0 == label_set_current(l));
    label_free(l);

    *gr->rvalp = 0;
    *gr->argp = arg;
    longjmp(gr->return_jmpbuf, 1);
}

static int
gate_call_setup_return(struct gate_return *gr,
		       void *return_stack,
		       struct cobj_ref *return_gatep)
{
    struct ulabel *l_recv = label_alloc();
    if (l_recv == 0)
	return -E_NO_MEM;

    l_recv->ul_default = 3;
    int r = label_set_level(l_recv, gr->return_handle, 0, 1);
    if (r < 0)
	return r;

    struct ulabel *l_send = label_get_current();
    if (l_send == 0) {
	r = -E_NO_MEM;
	goto out;
    }

    r = label_set_level(l_send, gr->return_handle, LB_LEVEL_STAR, 1);
    if (r < 0)
	goto out;

    struct thread_entry te = {
	.te_entry = &gate_call_return,
	.te_stack = return_stack + PGSIZE,
	.te_arg = (uint64_t) gr,
    };

    r = sys_thread_get_as(&te.te_as);
    if (r < 0)
	goto out;

    uint64_t return_gate_ct = kobject_id_thread_ct;
    int64_t gate_id = sys_gate_create(return_gate_ct, &te,
				      l_recv, l_send, "return gate");
    if (gate_id < 0) {
	r = gate_id;
	goto out;
    }

    *return_gatep = COBJ(return_gate_ct, gate_id);

out:
    if (l_recv)
	label_free(l_recv);
    if (l_send)
	label_free(l_send);
    return r;
}

int
gate_call(struct cobj_ref gate, struct cobj_ref *argp)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    int r = sys_segment_resize(tseg, 1);
    if (r < 0) {
	if (gate_debug)
	    cprintf("gate_call: cannot resize thread segment: %s\n", e2s(r));
	return r;
    }

    void *thread_local_seg = 0;
    r = segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &thread_local_seg, 0);
    if (r < 0) {
	if (gate_debug)
	    cprintf("gate_call: cannot map thread segment: %s\n", e2s(r));
	return r;
    }

    struct cobj_ref return_state_obj;
    void *return_state = 0;
    r = segment_alloc(kobject_id_thread_ct, PGSIZE,
		      &return_state_obj, &return_state,
		      0, "gate return state");
    if (r < 0) {
	if (gate_debug)
	    cprintf("gate_call: cannot allocate return state: %s\n", e2s(r));
	goto out2;
    }

    struct jmp_buf back_from_call;
    struct gate_return *gr = return_state;
    atomic_set(&gr->return_count, 0);
    gr->rvalp = &r;
    gr->argp = argp;
    gr->return_jmpbuf = &back_from_call;
    gr->return_handle = sys_handle_create();
    if (gr->return_handle < 0) {
	cprintf("gate_call: cannot alloc return handle: %s\n",
	        e2s(gr->return_handle));
	r = gr->return_handle;
	goto out3;
    }

    struct cobj_ref return_gate;
    if (setjmp(&back_from_call) == 0) {
	r = gate_call_setup_return(gr, thread_local_seg, &return_gate);
	if (r < 0) {
	    cprintf("gate_call: cannot setup return gate: %s", e2s(r));
	    goto out3;
	}

	// Compute the target label
	uint64_t nents = 64;
	uint64_t gate_label_ents[nents];
	struct ulabel gate_label = { .ul_size = nents, .ul_ent = &gate_label_ents[0] };

	r = sys_gate_send_label(gate, &gate_label);
	if (r < 0) {
	    cprintf("gate_call: getting send label: %s", e2s(r));
	    goto out4;
	}

	r = gate_compute_max_label(&gate_label);
	if (r < 0) {
	    cprintf("gate_call: computing label: %s", e2s(r));
	    goto out4;
	}

	r = label_set_level(&gate_label, gr->return_handle, LB_LEVEL_STAR, 0);
	if (r < 0) {
	    cprintf("gate_call: granting return handle: %s", e2s(r));
	    goto out4;
	}

	if (gate_debug)
	    printf("gate_call: target label %s\n", label_to_string(&gate_label));

	// Invoke the gate
	struct gate_call_args *gate_args = thread_local_seg;
	gate_args->return_gate = return_gate;
	gate_args->arg = *argp;
	r = sys_gate_enter(gate, &gate_label, 0, 0);
	if (r == 0)
	    panic("gate_call: sys_gate_enter returned 0");
    }

out4:
    sys_obj_unref(return_gate);
out3:
    segment_unmap(return_state);
    sys_obj_unref(return_state_obj);
out2:
    segment_unmap(thread_local_seg);
    return r;
}
