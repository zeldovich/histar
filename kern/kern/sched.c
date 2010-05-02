#include <kern/thread.h>
#include <kern/sched.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/container.h>
#include <kern/sync.h>
#include <kern/arch.h>
#include <inc/error.h>
#include <kern/reserve.h>
#include <kern/limit.h>
#include <kern/energy.h>
#include <kern/kobj.h>

static uint64_t global_tickets;
static uint64_t global_pass;
static uint64_t stride1;

static void
global_pass_update(uint64_t new_global_pass)
{
    static uint64_t last_tsc;

    uint64_t elapsed = karch_get_tsc() - last_tsc;
    last_tsc += elapsed;

    if (new_global_pass) {
	global_pass = new_global_pass;
    } else if (global_tickets) {
	global_pass += (stride1 / global_tickets) * elapsed;
    }
}

static int
sched_pass_below(uint64_t pass, uint64_t thresh)
{
    /* Wrap-around arithmetic..  never compare two pass values directly */
    int64_t delta = pass - thresh;
    return (delta < 0) ? 1 : 0;
}

static void
bill_energy()
{
    static uint64_t last_nsec;

    uint64_t now = timer_user_nsec();
    uint64_t elapsed = now - last_nsec;
    if (cur_thread) {
	if (last_nsec) {
	    // TODO need to ensure this succeeds once accounting is mandatory
	    thread_bill_energy(&kobject_dirty(&cur_thread->th_ko)->th,
			       energy_cpu_mJ(elapsed));
	}
    }
    last_nsec = now;
}

void
schedule(void)
{
    sync_wakeup_timer();
    timer_periodic_notify();

    bill_energy();
    // limit_update_all actually does decay just before distributing energy
    // also debit for baseline power consumption there as well
    limit_update_all();

    do {
	const struct Thread *t, *min_pass_th = 0;
	LIST_FOREACH(t, &thread_list_runnable, th_link)
	    if (!min_pass_th ||
		sched_pass_below(t->th_sched_pass, min_pass_th->th_sched_pass)) {
		// NOTICE thread_has_energy(t) neg (but true) if no reserve on thread
		// 0 if no energy
		// 1 if energy
		// hence this allows threads with no res or threads with filled res to run
		struct Thread *dt = &kobject_dirty(&t->th_ko)->th;
		int r = thread_has_energy(dt);
		// Adjust pass of thread that can't run due to lack of energy
		// so that when it can run again it can't starve others
		dt->th_sched_pass = global_pass + dt->th_sched_remain;
		if (r)
		    min_pass_th = t;
	    }

	cur_thread = min_pass_th;
	if (!cur_thread) {
	    // Schedule a preemption timer, 10 msec quantum
	    the_schedtmr->schedule_nsec(the_schedtmr->arg, 10 * 1000 * 1000);
	    return;
	}

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

    bill_energy();
}

void
sched_stop(struct Thread *t, uint64_t elapsed)
{
    uint64_t tickets = t->th_sched_tickets ? : 1;
    uint64_t th_stride = stride1 / tickets;
    t->th_sched_pass += th_stride * elapsed;
}

void
sched_init(void)
{
    stride1 = UINT64(1) << 32;
}
