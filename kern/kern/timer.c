#include <machine/types.h>
#include <machine/thread.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <dev/kclock.h>
#include <inc/queue.h>

uint64_t timer_ticks;
struct Thread_list timer_sleep;

static LIST_HEAD(pt_list, periodic_task) periodic_tasks;

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
    timer_ticks++;

    struct periodic_task *pt;
    LIST_FOREACH(pt, &periodic_tasks, pt_link) {
	if (pt->pt_wakeup_ticks < timer_ticks) {
	    pt->pt_fn();
	    pt->pt_wakeup_ticks = timer_ticks + pt->pt_interval_ticks;
	}
    }
}

void
timer_add_periodic(struct periodic_task *pt)
{
    pt->pt_wakeup_ticks = 0;
    LIST_INSERT_HEAD(&periodic_tasks, pt, pt_link);
}

void
timer_init(void)
{
    static struct interrupt_handler timer_ih = { .ih_func = &timer_intr };

    irq_register(0, &timer_ih);
    LIST_INIT(&timer_sleep);

    static struct periodic_task sleep_pt =
	{ .pt_fn = &wakeup_scan, .pt_interval_ticks = 1 };
    timer_add_periodic(&sleep_pt);
}
