#include <machine/x86.h>
#include <kern/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/container.h>
#include <inc/error.h>

static uint128_t global_tickets;
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
	uint128_t s1 = stride1;
	global_pass += s1 * elapsed / global_tickets;
    }
}

void
schedule(void)
{
    do {
	const struct Thread *t, *min_pass_th = 0;
	LIST_FOREACH(t, &thread_list_runnable, th_link)
	    if (!min_pass_th || t->th_sched_pass < min_pass_th->th_sched_pass)
		min_pass_th = t;

	if (!min_pass_th)
	    panic("no runnable threads");

	cur_thread = min_pass_th;

	// Halt thread if it can't know of its existence..
	thread_check_sched_parents(cur_thread);
    } while (!cur_thread || !SAFE_EQUAL(cur_thread->th_status, thread_runnable));

    // Make sure we don't miss a TSC rollover, and reset it just in case
    global_pass_update(cur_thread->th_sched_pass);
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
sched_start(const struct Thread *t __attribute__((unused)))
{
    cur_start_tsc = read_tsc();
}

void
sched_stop(struct Thread *t)
{
    uint64_t elapsed_tsc = read_tsc() - cur_start_tsc;
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

    static struct periodic_task sched_pt =
	{ .pt_fn = &schedule, .pt_interval_ticks = 1 };
    timer_add_periodic(&sched_pt);
}
