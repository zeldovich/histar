#include <inc/lib.h>
#include <inc/setjmp.h>
#include <inc/syscall.h>
#include <inc/assert.h>

int
gate_call(uint64_t ctemp, struct cobj_ref gate)
{
    // XXX
    // need to prevent multiple returns via the return gate
    // by allocating a new stack, storing an atomic_t counter
    // on that stack, and unref'ing the stack when we're done
    // to ensure that future gate entrants will pagefault.
    // 
    // unref the rgate first, then the stack.

    struct cobj_ref rgate;
    struct jmp_buf jmp;
    int returned = 0;

    setjmp(&jmp);
    if (returned) {
	sys_obj_unref(rgate);
	return 0;
    }
    returned++;

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

    int slot = sys_gate_create(ctemp, &te, &ul, &ul);
    if (slot < 0)
	return slot;

    rgate = COBJ(ctemp, slot);
    r = sys_gate_enter(gate, rgate.container, rgate.slot);
    sys_obj_unref(rgate);

    if (r < 0)
	return r;

    panic("sys_gate_enter returned 0");
}
