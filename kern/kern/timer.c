#include <machine/types.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <dev/kclock.h>
#include <inc/queue.h>

uint64_t timer_user_nsec_offset;
struct time_source *the_timesrc;
struct preemption_timer *the_schedtmr;

uint64_t
timer_convert(uint64_t n, uint64_t a, uint64_t b)
{
    uint64_t hi = n >> 32;
    uint64_t lo = n & ((UINT64(1) << 32) - 1);

    return ((hi * a / b) << 32) + (lo * a / b);
}

uint64_t
timer_user_nsec(void)
{
    return timer_convert(the_timesrc->ticks(the_timesrc->arg),
			 1000000000, the_timesrc->freq_hz) +
	   timer_user_nsec_offset;
}

void
timer_delay(uint64_t nsec)
{
    the_timesrc->delay_nsec(the_timesrc->arg, nsec);
}

/*
 * Periodic task handling.
 */
static LIST_HEAD(pt_list, periodic_task) periodic_tasks;

void timer_periodic_notify(void)
{
    uint64_t ticks = the_timesrc->ticks(the_timesrc->arg);

    struct periodic_task *pt;
    LIST_FOREACH(pt, &periodic_tasks, pt_link) {
	if (ticks - pt->pt_last_ticks >= pt->pt_interval_ticks) {
	    pt->pt_fn();
	    pt->pt_last_ticks = ticks;
	}
    }
}

void
timer_add_periodic(struct periodic_task *pt)
{
    pt->pt_last_ticks = the_timesrc->ticks(the_timesrc->arg);
    pt->pt_interval_ticks = pt->pt_interval_sec * the_timesrc->freq_hz;
    LIST_INSERT_HEAD(&periodic_tasks, pt, pt_link);
}

void
timer_remove_periodic(struct periodic_task *pt)
{
    LIST_REMOVE(pt, pt_link);
}
