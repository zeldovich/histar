#include <machine/thread.h>
#include <machine/x86.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>

static uint128_t global_tickets;
static uint128_t global_pass;
static uint64_t stride1;
static uint64_t cur_start_tsc;

static uint64_t
sched_tickets(const struct Thread *t)
{
    return 1024;
}

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
    const struct Thread *t, *min_pass_th = 0;
    LIST_FOREACH(t, &thread_list_runnable, th_link)
	if (!min_pass_th || t->th_sched_pass < min_pass_th->th_sched_pass)
	    min_pass_th = t;

    if (!min_pass_th)
	panic("no runnable threads");

    // Make sure we don't miss a TSC rollover, and reset it just in case
    global_pass_update(min_pass_th->th_sched_pass);

    cur_thread = min_pass_th;
}

void
sched_join(struct Thread *t)
{
    global_pass_update(0);

    t->th_sched_pass = global_pass + t->th_sched_remain;
    global_tickets += sched_tickets(t);
}

void
sched_leave(struct Thread *t)
{
    global_pass_update(0);

    t->th_sched_remain = t->th_sched_pass - global_pass;
    global_tickets -= sched_tickets(t);
}

void
sched_start(const struct Thread *t)
{
    cur_start_tsc = read_tsc();
}

void
sched_stop(struct Thread *t)
{
    uint64_t elapsed_tsc = read_tsc() - cur_start_tsc;
    uint64_t tickets = sched_tickets(t) ? : 1;
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
