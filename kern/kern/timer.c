#include <machine/types.h>
#include <kern/timer.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <kern/sync.h>
#include <dev/kclock.h>
#include <inc/queue.h>

static uint64_t timer_ticks;
uint64_t timer_user_msec_offset;
uint64_t timer_user_msec;

static LIST_HEAD(pt_list, periodic_task) periodic_tasks;

static void
timer_intr(void)
{
    timer_ticks++;
    timer_user_msec = timer_user_msec_offset +
		      kclock_ticks_to_msec(timer_ticks);

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

    static struct periodic_task sync_timer_pt =
	{ .pt_fn = &sync_wakeup_timer, .pt_interval_ticks = 1 };
    timer_add_periodic(&sync_timer_pt);
}
