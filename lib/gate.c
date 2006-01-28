#include <inc/lib.h>
#include <inc/gate.h>
#include <inc/setjmp.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/atomic.h>
#include <inc/memlayout.h>
#include <inc/stack.h>

struct gate_call_args {
    struct cobj_ref return_gate;
    struct cobj_ref arg;
};

struct gate_entry_stack_info {
    struct cobj_ref obj;
    void *va;
};

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

    int r = sys_obj_unref(COBJ(ug->container, thread_id()));
    if (r < 0)
	panic("gate_return_entrystack: unref: %s", e2s(r));

    r = sys_gate_enter(return_gate, arg.container, arg.object);
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

    int stackpages = 2;

    struct gate_entry_stack_info stackinf;
    stackinf.va = 0;
    r = segment_alloc(ug->container, stackpages * PGSIZE,
		      &stackinf.obj, &stackinf.va);
    if (r < 0)
	panic("gate_entry_locked: cannot allocate new stack: %s", e2s(r));

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(ug->gate, &name[0]);
    if (r < 0)
	panic("gate_entry_locked: sys_obj_get_name: %s", e2s(r));

    char sname[KOBJ_NAME_LEN];
    snprintf(&sname[0], KOBJ_NAME_LEN, "gate stack for %s", name);
    r = sys_obj_set_name(stackinf.obj, &sname[0]);
    if (r < 0)
	panic("gate_entry_locked: sys_obj_set_name: %s", e2s(r));

    stack_switch((uint64_t) ug, (uint64_t) &stackinf, 0, 0,
		 stackinf.va + stackpages * PGSIZE,
		 &gate_entry_newstack);
}

int
gate_create(struct u_gate_entry *ug, uint64_t container,
	    void (*func) (void*, struct cobj_ref*), void *func_arg,
	    char *name)
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
    struct ulabel ul = { .ul_size = 8, .ul_ent = &label_ents[0], };

    r = thread_get_label(container, &ul);
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

    int64_t gate_id = sys_gate_create(container, &te, &ul, &ul);
    if (gate_id < 0) {
	r = gate_id;
	goto out;
    }

    ug->gate = COBJ(container, gate_id);
    sys_obj_set_name(ug->gate, name);
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
};

static void __attribute__((noreturn))
gate_call_return(struct gate_return *gr, struct cobj_ref arg)
{
    if (atomic_compare_exchange(&gr->return_count, 0, 1) != 0)
	panic("gate_call_return: multiple return");

    *gr->rvalp = 0;
    *gr->argp = arg;
    longjmp(gr->return_jmpbuf, 1);
}

static int
gate_call_setup_return(uint64_t ctemp, struct gate_return *gr,
		       void *return_stack,
		       struct cobj_ref *return_gatep)
{
    uint64_t label_ents[8];
    struct ulabel ul = { .ul_size = 8, .ul_ent = &label_ents[0], };

    int r = thread_get_label(ctemp, &ul);
    if (r < 0)
	return r;

    struct thread_entry te = {
	.te_entry = &gate_call_return,
	.te_stack = return_stack + PGSIZE,
	.te_arg = (uint64_t) gr,
    };

    r = sys_thread_get_as(&te.te_as);
    if (r < 0)
	return r;

    int64_t gate_id = sys_gate_create(ctemp, &te, &ul, &ul);
    if (gate_id < 0)
	return gate_id;

    *return_gatep = COBJ(ctemp, gate_id);
    sys_obj_set_name(*return_gatep, "return gate");
    return 0;
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
    r = segment_alloc(ctemp, PGSIZE, &return_stack_obj, &return_stack);
    if (r < 0)
	goto out2;

    sys_obj_set_name(return_stack_obj, "gate return stack");

    struct jmp_buf back_from_call;
    struct gate_return *gr = return_stack;
    atomic_set(&gr->return_count, 0);
    gr->rvalp = &r;
    gr->argp = argp;
    gr->return_jmpbuf = &back_from_call;

    struct cobj_ref return_gate;
    if (setjmp(&back_from_call) == 0) {
	r = gate_call_setup_return(ctemp, gr, return_stack, &return_gate);
	if (r < 0)
	    goto out3;

	gate_args->return_gate = return_gate;
	gate_args->arg = *argp;
	r = sys_gate_enter(gate, 0, 0);
	if (r == 0)
	    panic("gate_call: sys_gate_enter returned 0");
    }

    sys_obj_unref(return_gate);
out3:
    segment_unmap(return_stack);
    sys_obj_unref(return_stack_obj);
out2:
    segment_unmap(gate_args);
    return r;
}
