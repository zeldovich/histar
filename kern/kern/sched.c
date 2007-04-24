#include <machine/x86.h>
#include <kern/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/container.h>
#include <kern/sync.h>
#include <inc/error.h>

static uint64_t global_tickets;
static uint128_t global_pass;
static uint64_t stride1;
static uint64_t cur_start_tsc;

static void
global_pass_update(uint128_t new_global_pass)
{
    static uint64_t last_tsc;

    uint64_t elapsed = read_tsc() - last_tsc;
    last_tsc += elapsed;

    if (new_global_pass) {
	global_pass = new_global_pass;
    } else if (global_tickets) {
	global_pass += ((uint128_t) (stride1 / global_tickets)) * elapsed;
    }
}

void
schedule(void)
{
    sync_wakeup_timer();
    timer_periodic_notify();

    do {
	const struct Thread *t, *min_pass_th = 0;
	LIST_FOREACH(t, &thread_list_runnable, th_link)
	    if (!min_pass_th ||
		(t->th_sched_pass < min_pass_th->th_sched_pass &&
		 !(t->th_ko.ko_flags & KOBJ_PIN_IDLE)))
		min_pass_th = t;

	if (!min_pass_th)
	    panic("no runnable threads");

	cur_thread = min_pass_th;
	assert(SAFE_EQUAL(cur_thread->th_status, thread_runnable));

	// Halt thread if it can't know of its existence..
	thread_check_sched_parents(cur_thread);
    } while (!cur_thread || !SAFE_EQUAL(cur_thread->th_status, thread_runnable));

    // Make sure we don't miss a TSC rollover, and reset it just in case
    global_pass_update(cur_thread->th_sched_pass);

    // Schedule a preemption timer, 10 msec quantum
    the_schedtmr->schedule_nsec(the_schedtmr->arg, 10 * 1000 * 1000);
}

void
sched_join(struct Thread *t)
{
    global_pass_update(0);

    t->th_sched_pass = global_pass + t->th_sched_remain;
    global_tickets += t->th_sched_tickets;
}

void
sched_leave(struct Thread *t)
{
    global_pass_update(0);

    t->th_sched_remain = t->th_sched_pass - global_pass;
    global_tickets -= t->th_sched_tickets;
}

void
sched_start(const struct Thread *t, uint64_t tsc)
{
    cur_start_tsc = tsc;
}

void
sched_stop(struct Thread *t, uint64_t tsc)
{
    uint64_t elapsed_tsc = tsc - cur_start_tsc;
    uint64_t tickets = t->th_sched_tickets ? : 1;
    uint128_t th_stride = stride1 / tickets;
    t->th_sched_pass += th_stride * elapsed_tsc;
}

void
sched_init(void)
{
    // Set stride1 to all-ones
    stride1 = 0;
    stride1--;
}
