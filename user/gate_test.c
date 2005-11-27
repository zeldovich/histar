#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/stdio.h>

static void
gate_entry(uint64_t arg)
{
    cprintf("gate_entry: %lx\n", arg);
    sys_halt();
}

int
main(int ac, char **av)
{
    // root container, always 0 for now (sequential alloc)
    int rc = 0;

    int as = sys_container_store_cur_addrspace(rc, 0);
    if (as < 0)
	panic("cannot store cur_as: %d", as);

    int g = sys_gate_create(rc, &gate_entry, 0xc0ffee00c0ffee, COBJ(rc, as));
    if (g < 0)
	panic("cannot create gate: %d", g);

    int r = sys_gate_enter(COBJ(rc, g));
    if (r < 0)
	panic("cannot enter gate: %d", r);

    panic("still alive after gate_enter");
}
