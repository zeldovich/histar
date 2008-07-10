#include <kern/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/container.h>
#include <kern/kobj.h>
#include <kern/sync.h>
#include <kern/arch.h>
#include <inc/error.h>

extern uint64_t user_root_ct;

const uint64_t stride1 = ((uint64_t) 0) - 1;

void
schedule(void)
{
    const struct Container *rct;
    int r;

    sync_wakeup_timer();
    timer_periodic_notify();

    cprintf("*** schedule\n");

    r = container_find(&rct, user_root_ct, iflow_none);
    if (r < 0)
        panic("schedule: Could not schedule the root container");
    do {
        cprintf("!");
        r = container_schedule(rct);
        if (r < 0) {
            cprintf("schedule: failed to schedule a runnable thread\n");
            // TODO: remove the panic before done
            panic("schedule: failed to schedule a runnable thread\n");
            the_schedtmr->schedule_nsec(the_schedtmr->arg, 10 * 1000 * 1000);
            return;
        }
	assert(SAFE_EQUAL(cur_thread->th_status, thread_runnable));

	// Halt thread if it can't know of its existence..
        // TODO: Is this still needed this way?
	thread_check_sched_parents(cur_thread);
    } while (!cur_thread || !SAFE_EQUAL(cur_thread->th_status, thread_runnable));
    // Make sure we don't miss a TSC rollover, and reset it just in case
    // TODO: What does this become?
    // global_pass_update(cur_thread->th_sched_pass);

    // Schedule a preemption timer, 10 msec quantum
    the_schedtmr->schedule_nsec(the_schedtmr->arg, 10 * 1000 * 1000);
}

