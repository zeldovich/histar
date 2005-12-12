#include <machine/types.h>
#include <machine/thread.h>
#include <kern/timer.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/pstate.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <inc/queue.h>

uint64_t timer_ticks;
struct Thread_list timer_sleep;

static void
wakeup_scan(void)
{
    struct Thread *t = LIST_FIRST(&timer_sleep);
    while (t != 0) {
	struct Thread *next = LIST_NEXT(t, th_link);

	if (t->th_wakeup_ticks < timer_ticks)
	    thread_set_runnable(t);

	t = next;
    }
}

static void
timer_intr(void)
{
    kobject_gc_scan();
    wakeup_scan();

    timer_ticks++;
    if (!(timer_ticks % 1000))
	pstate_sync();

    schedule();
}

void
timer_init(void)
{
    static struct interrupt_handler timer_ih = { .ih_func = &timer_intr };

    irq_register(0, &timer_ih);
    LIST_INIT(&timer_sleep);
}
