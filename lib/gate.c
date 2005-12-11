#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/atomic.h>
#include <inc/memlayout.h>

static int
gate_call_unsafe(uint64_t ctemp, struct cobj_ref gate)
{
    int returned;
    struct cobj_ref rgate;
    struct jmp_buf jmp;

    returned = 0;
    setjmp(&jmp);
    if (returned) {
	sys_obj_unref(rgate);
	return 0;
    }
    returned = 1;

    uint64_t label_ents[8];
    struct ulabel ul = {
	.ul_size = 8,
	.ul_ent = &label_ents[0],
    };

    int r = thread_get_label(ctemp, &ul);
    if (r < 0)
	return r;

    struct thread_entry te = {
	.te_entry = longjmp,
	.te_arg = (uint64_t) &jmp,
    };

    r = sys_segment_get_map(&te.te_segmap);
    if (r < 0)
	return r;

    int64_t gate_id = sys_gate_create(ctemp, &te, &ul, &ul);
    if (gate_id < 0)
	return gate_id;

    rgate = COBJ(ctemp, gate_id);
    r = sys_gate_enter(gate, rgate.container, rgate.object);
    sys_obj_unref(rgate);

    if (r < 0)
	return r;

    panic("sys_gate_enter returned 0");
}

static void __attribute__((noreturn))
gate_call_newstack(uint64_t ctemp, struct cobj_ref gate,
		   struct jmp_buf *back, int *rvalp)
{
    atomic_t return_count = ATOMIC_INIT(0);

    int rval = gate_call_unsafe(ctemp, gate);
    if (atomic_compare_exchange(&return_count, 0, 1) == 0) {
	*rvalp = rval;
	longjmp(back, 1);
    }

    panic("gate_call_newstack: multiple return");
}

int
gate_call(uint64_t ctemp, struct cobj_ref gate)
{
    struct cobj_ref temp_stack_obj;
    int r = segment_alloc(ctemp, PGSIZE, &temp_stack_obj);
    if (r < 0)
	return r;

    void *temp_stack_va;
    r = segment_map(ctemp, temp_stack_obj, 1, &temp_stack_va, 0);
    if (r < 0) {
	sys_obj_unref(temp_stack_obj);
	return r;
    }

    int rval;
    struct jmp_buf back_from_newstack;
    if (setjmp(&back_from_newstack) != 0) {
	segment_unmap(ctemp, temp_stack_va);
	sys_obj_unref(temp_stack_obj);
	return rval;
    }

    struct jmp_buf jump_to_newstack;
    if (setjmp(&jump_to_newstack) != 0)
	gate_call_newstack(ctemp, gate, &back_from_newstack, &rval);

    // XXX not particularly machine-independent..
    jump_to_newstack.jb_rsp = (uintptr_t)temp_stack_va + PGSIZE;
    longjmp(&jump_to_newstack, 1);
}
