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

/*
 * timer_convert() assumes both a and b are on the order of 1<<32.
 */
uint64_t
timer_convert(uint64_t n, uint64_t a, uint64_t b)
{
    uint64_t hi = n >> 32;
    uint64_t lo = n & 0xffffffff;

    uint64_t hi_hz = hi * a;
    uint64_t hi_b = hi_hz / b;
    uint64_t hi_hz_carry = hi_hz - hi_b * b;

    uint64_t lo_hz = lo * a + (hi_hz_carry << 32);
    uint64_t lo_b = lo_hz / b;

    return (hi_b << 32) + lo_b;
}

uint64_t
timer_user_nsec(void)
{
    assert(the_timesrc);

    uint64_t ticks = the_timesrc->ticks(the_timesrc->arg);
    uint64_t nsec = timer_convert(ticks, 1000000000, the_timesrc->freq_hz);
    return nsec + timer_user_nsec_offset;
}

void
timer_delay(uint64_t nsec)
{
    if (the_timesrc)
	the_timesrc->delay_nsec(the_timesrc->arg, nsec);
}

/*
 * Periodic task handling.
 */
static LIST_HEAD(pt_list, periodic_task) periodic_tasks;

void timer_periodic_notify(void)
{
    assert(the_timesrc);
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
    assert(the_timesrc);

    pt->pt_last_ticks = the_timesrc->ticks(the_timesrc->arg);
    pt->pt_interval_ticks = pt->pt_interval_sec * the_timesrc->freq_hz;
    LIST_INSERT_HEAD(&periodic_tasks, pt, pt_link);
}

void
timer_remove_periodic(struct periodic_task *pt)
{
    LIST_REMOVE(pt, pt_link);
}
