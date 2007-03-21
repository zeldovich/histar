#include <machine/types.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <dev/kclock.h>
#include <inc/queue.h>

uint64_t timer_user_nsec_offset;
struct hw_timer *the_timer;

uint64_t
timer_user_nsec(void)
{
    uint64_t ticks = the_timer->ticks(the_timer->arg);

    uint64_t ticks_hi = ticks >> 32;
    uint64_t ticks_lo = ticks & ((UINT64(1) << 32) - 1);

    return ((ticks_hi * 1000000000 / the_timer->freq_hz) << 32) +
	    (ticks_lo * 1000000000 / the_timer->freq_hz) +
	   timer_user_nsec_offset;
}

void
timer_delay(uint64_t nsec)
{
    the_timer->delay(the_timer->arg, nsec);
}

/*
 * Periodic task handling.
 */
static LIST_HEAD(pt_list, periodic_task) periodic_tasks;

void timer_periodic_notify(void)
{
    uint64_t ticks = the_timer->ticks(the_timer->arg);

    struct periodic_task *pt;
    LIST_FOREACH(pt, &periodic_tasks, pt_link) {
	uint64_t waitsec = (ticks - pt->pt_last_ticks) / the_timer->freq_hz;
	if (waitsec >= pt->pt_interval_sec) {
	    pt->pt_fn();
	    pt->pt_last_ticks = ticks;
	}
    }
}

void
timer_add_periodic(struct periodic_task *pt)
{
    pt->pt_last_ticks = the_timer->ticks(the_timer->arg);
    LIST_INSERT_HEAD(&periodic_tasks, pt, pt_link);
}

void
timer_remove_periodic(struct periodic_task *pt)
{
    LIST_REMOVE(pt, pt_link);
}
