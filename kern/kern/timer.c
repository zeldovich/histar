#include <machine/types.h>
#include <machine/thread.h>
#include <kern/timer.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/pstate.h>
#include <kern/kobj.h>
#include <inc/queue.h>

uint64_t timer_ticks;
struct Thread_tqueue timer_sleep_tqueue;

static void
wakeup_scan()
{
    struct Thread *t = TAILQ_FIRST(&timer_sleep_tqueue);
    while (t != 0) {
	struct Thread *next = TAILQ_NEXT(t, th_waiting);

	if (t->th_wakeup_ticks < timer_ticks) {
	    thread_set_runnable(t);
	    TAILQ_REMOVE(&timer_sleep_tqueue, t, th_waiting);
	}

	t = next;
    }
}

void
timer_intr()
{
    kobject_gc_scan();
    wakeup_scan();

    timer_ticks++;
    if (!(timer_ticks % 1000))
	pstate_sync();

    schedule();
}

void
timer_init()
{
    TAILQ_INIT(&timer_sleep_tqueue);
}
