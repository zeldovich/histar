#include <machine/types.h>
#include <kern/timer.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/pstate.h>

static uint64_t ticks = 0;

void
timer_intr()
{
    kobject_gc_scan();

    ticks++;
    if (!(ticks % 1000))
	pstate_sync();

    schedule();
}
