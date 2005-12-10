#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/lib.h>

static void
gate_entry(char *arg)
{
    cprintf("gate_entry: %s\n", arg);
    thread_halt();
}

int
main(int ac, char **av)
{
    cprintf("server process starting.\n");

    int rc = 1;		// abuse the root container
    int myct = start_arg;

    struct cobj_ref stack_seg;
    int stacksize = 2 * PGSIZE;
    assert(0 == segment_alloc(myct, stacksize, &stack_seg));
    void *stack_va;
    assert(0 == segment_map(myct, stack_seg, 1, &stack_va, 0));

    struct thread_entry te;
    assert(0 == sys_segment_get_map(&te.te_segmap));
    te.te_entry = &gate_entry;
    te.te_stack = stack_va + stacksize;

    char *arg = "Hello world.";
    te.te_arg = (uint64_t) arg;

    uint64_t ul_ent[4];
    struct ulabel ul;
    ul.ul_size = 4;
    ul.ul_ent = &ul_ent[0];

    assert(0 == thread_get_label(myct, &ul));

    int slot = sys_gate_create(rc, &te, &ul, &ul);
    if (slot < 0)
	panic("cannot create gate: %s", e2s(slot));
}
