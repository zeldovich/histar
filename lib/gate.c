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

    gate_return(ug, return_gate, arg);
}

static void __attribute__((noreturn))
gate_entry_newstack(struct u_gate_entry *ug, struct cobj_ref call_args_obj,
		    struct gate_entry_stack_info *stackinfp)
{
    struct gate_entry_stack_info stackinf = *stackinfp;
    atomic_set(&ug->entry_stack_use, 0);

    struct gate_call_args *call_args = 0;
    int r = segment_map(call_args_obj, SEGMAP_READ, (void**) &call_args, 0);
    if (r < 0)
	panic("gate_entry: cannot map argument page: %s", e2s(r));

    struct cobj_ref arg = call_args->arg;
    struct cobj_ref return_gate = call_args->return_gate;

    r = segment_unmap(call_args);
    if (r < 0)
	panic("gate_entry: cannot unmap argument page: %s", e2s(r));

    ug->func(ug->func_arg, &arg);

    while (atomic_compare_exchange(&ug->entry_stack_use, 0, 1) != 0)
	sys_thread_yield();

    stack_switch((uint64_t) ug,
		 (uint64_t) &return_gate,
		 (uint64_t) &arg,
		 (uint64_t) &stackinf,
		 ug->stackbase + PGSIZE,
		 &gate_return_entrystack);
}

// The assembly gate_entry code jumps here, once the thread has been
// addref'ed to the container and the entry stack has been locked.
void __attribute__((noreturn))
gate_entry_locked(struct u_gate_entry *ug, struct cobj_ref call_args_obj)
{
    int stackpages = 2;

    struct gate_entry_stack_info stackinf;
    stackinf.va = 0;
    int r = segment_alloc(ug->container, stackpages * PGSIZE,
			  &stackinf.obj, &stackinf.va);
    if (r < 0)
	panic("gate_entry_locked: cannot allocate new stack: %s", e2s(r));

    stack_switch((uint64_t) ug,
		 call_args_obj.container,
		 call_args_obj.object,
		 (uint64_t) &stackinf,
		 stackinf.va + stackpages * PGSIZE,
		 &gate_entry_newstack);
}

int
gate_create(struct u_gate_entry *ug, uint64_t container,
	    void (*func) (void*, struct cobj_ref*), void *func_arg)
{
    ug->stackbase = 0;
    int r = segment_alloc(container, PGSIZE,
			  &ug->stackpage, &ug->stackbase);
    if (r < 0)
	return r;

    atomic_set(&ug->entry_stack_use, 0);
    ug->container = container;
    ug->func = func;
    ug->func_arg = func_arg;

    uint64_t label_ents[8];
    struct ulabel ul = { .ul_size = 8, .ul_ent = &label_ents[0], };

    r  = thread_get_label(container, &ul);
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
    return 0;

out:
    sys_obj_unref(ug->stackpage);
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
    return 0;
}

int
gate_call(uint64_t ctemp, struct cobj_ref gate, struct cobj_ref *argp)
{
    struct cobj_ref gate_args_obj;
    struct gate_call_args *gate_args = 0;
    int r = segment_alloc(ctemp, PGSIZE, &gate_args_obj, (void**) &gate_args);
    if (r < 0)
	goto out1;

    struct cobj_ref return_stack_obj;
    void *return_stack = 0;
    r = segment_alloc(ctemp, PGSIZE, &return_stack_obj, &return_stack);
    if (r < 0)
	goto out2;

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
	r = sys_gate_enter(gate, gate_args_obj.container,
				 gate_args_obj.object);
	if (r == 0)
	    panic("gate_call: sys_gate_enter returned 0");
    }

    sys_obj_unref(return_gate);
out3:
    segment_unmap(return_stack);
    sys_obj_unref(return_stack_obj);
out2:
    segment_unmap(gate_args);
    sys_obj_unref(gate_args_obj);
out1:
    return r;
}
