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

    int pm = sys_container_store_cur_pmap(rc, 0);
    if (pm < 0)
	panic("cannot store cur_pm: %d", pm);

    // XXX if we could get a user-space header defining ULIM...
    char *stacktop = (void*) 0x700000000000;
    int sg = sys_segment_create(rc, 1);
    if (sg < 0)
	panic("cannot create stack segment: %d", sg);

    int r = sys_segment_map(COBJ(rc, sg), COBJ(rc, pm), stacktop - 4096, 0, 1, segment_map_cow);
    if (r < 0)
	panic("cannot map stack segment: %d", r);

    int g = sys_gate_create(rc, &gate_entry, stacktop, COBJ(rc, pm));
    if (g < 0)
	panic("cannot create gate: %d", g);

    r = sys_gate_enter(COBJ(rc, g));
    if (r < 0)
	panic("cannot enter gate: %d", r);

    panic("still alive after gate_enter");
}
