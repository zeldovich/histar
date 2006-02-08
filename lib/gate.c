#include <inc/lib.h>
#include <inc/gate.h>
#include <inc/setjmp.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/atomic.h>
#include <inc/memlayout.h>
#include <inc/stack.h>
#include <inc/error.h>

struct gate_call_args {
    struct cobj_ref return_gate;
    struct cobj_ref arg;
};

struct gate_entry_stack_info {
    struct cobj_ref obj;
    void *va;
};

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

    r = sys_gate_enter(return_gate, &gate_label, arg.container, arg.object);
    panic("gate_return_entrystack: gate_enter: %s", e2s(r));
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
gate_create(struct u_gate_entry *ug, uint64_t container,
	    void (*func) (void*, struct cobj_ref*), void *func_arg,
	    const char *name)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    int r = sys_segment_resize(tseg, 1);
    if (r < 0)
	return r;

    ug->stackbase = 0;
    r = segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, &ug->stackbase, 0);
    if (r < 0)
	return r;

    ug->container = container;
    ug->func = func;
    ug->func_arg = func_arg;

    uint64_t label_ents[8];
    struct ulabel l_send = { .ul_size = 8, .ul_ent = &label_ents[0], };
    struct ulabel l_recv = { .ul_nent = 0, .ul_default = 2 };

    r = thread_get_label(container, &l_send);
    if (r < 0)
	goto out;

    struct thread_entry te = {
	.te_entry = &gate_entry,
	.te_stack = ug->stackbase + PGSIZE,
	.te_arg = (uint64_t) ug,
    };

    r = sys_thread_get_as(&te.te_as);
    if (r < 0)
	goto out;

    int64_t gate_id = sys_gate_create(container, &te,
				      &l_recv, &l_send, name);
    if (gate_id < 0) {
	r = gate_id;
	goto out;
    }

    ug->gate = COBJ(container, gate_id);
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
    if (atomic_compare_exchange(&gr->return_count, 0, 1) != 0)
	panic("gate_call_return: multiple return");

    struct ulabel *l = label_get_current();
    assert(l);

    assert(0 == label_set_level(l, gr->return_handle, l->ul_default, 1));
    assert(0 == label_set_current(l));

    *gr->rvalp = 0;
    *gr->argp = arg;
    longjmp(gr->return_jmpbuf, 1);
}

static int
gate_call_setup_return(uint64_t ctemp, struct gate_return *gr,
		       void *return_stack,
		       struct cobj_ref *return_gatep)
{
    struct ulabel *l_recv = label_alloc();
    if (l_recv == 0)
	return -E_NO_MEM;

    l_recv->ul_default = 2;
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

    int64_t gate_id = sys_gate_create(ctemp, &te, l_recv, l_send, "return gate");
    if (gate_id < 0) {
	r = gate_id;
	goto out;
    }

    *return_gatep = COBJ(ctemp, gate_id);

out:
    if (l_recv)
	label_free(l_recv);
    if (l_send)
	label_free(l_send);
    return r;
}

int
gate_call(uint64_t ctemp, struct cobj_ref gate, struct cobj_ref *argp)
{
    struct cobj_ref tseg = COBJ(kobject_id_thread_ct, kobject_id_thread_sg);
    int r = sys_segment_resize(tseg, 1);
    if (r < 0)
	return r;

    struct gate_call_args *gate_args = 0;
    r = segment_map(tseg, SEGMAP_READ | SEGMAP_WRITE, (void**)&gate_args, 0);
    if (r < 0)
	return r;

    struct cobj_ref return_stack_obj;
    void *return_stack = 0;
    r = segment_alloc(ctemp, PGSIZE, &return_stack_obj, &return_stack,
		      0, "gate return stack");
    if (r < 0)
	goto out2;

    struct jmp_buf back_from_call;
    struct gate_return *gr = return_stack;
    atomic_set(&gr->return_count, 0);
    gr->rvalp = &r;
    gr->argp = argp;
    gr->return_jmpbuf = &back_from_call;
    gr->return_handle = sys_handle_create();
    if (gr->return_handle < 0) {
	printf("gate_call: cannot alloc return handle: %s\n",
	       e2s(gr->return_handle));
	r = gr->return_handle;
	goto out3;
    }

    struct cobj_ref return_gate;
    if (setjmp(&back_from_call) == 0) {
	r = gate_call_setup_return(ctemp, gr, return_stack, &return_gate);
	if (r < 0)
	    goto out3;

	// Compute the target label
	uint64_t nents = 64;
	uint64_t gate_label_ents[nents];
	struct ulabel gate_label = { .ul_size = nents, .ul_ent = &gate_label_ents[0] };

	r = sys_gate_send_label(gate, &gate_label);
	if (r < 0) {
	    printf("gate_call: getting send label: %s", e2s(r));
	    goto out4;
	}

	r = gate_compute_max_label(&gate_label);
	if (r < 0) {
	    printf("gate_call: computing label: %s", e2s(r));
	    goto out4;
	}

	r = label_set_level(&gate_label, gr->return_handle, LB_LEVEL_STAR, 0);
	if (r < 0) {
	    printf("gate_call: granting return handle: %s", e2s(r));
	    goto out4;
	}

	// Invoke the gate
	gate_args->return_gate = return_gate;
	gate_args->arg = *argp;
	r = sys_gate_enter(gate, &gate_label, 0, 0);
	if (r == 0)
	    panic("gate_call: sys_gate_enter returned 0");
    }

out4:
    sys_obj_unref(return_gate);
out3:
    segment_unmap(return_stack);
    sys_obj_unref(return_stack_obj);
out2:
    segment_unmap(gate_args);
    return r;
}
