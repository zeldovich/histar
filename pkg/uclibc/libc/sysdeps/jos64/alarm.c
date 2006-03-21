#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/signal.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>

static struct cobj_ref alarm_worker_obj;
static uint64_t alarm_worker_ct;

static struct cobj_ref alarm_target_obj;

static atomic64_t alarm_at_msec;

static void __attribute__((noreturn))
alarm_worker(void *arg)
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_signo = SIGALRM;

    for (;;) {
	uint64_t now = sys_clock_msec();
	uint64_t alarm_at = atomic_read(&alarm_at_msec);

	if (now >= alarm_at) {
	    uint64_t old = atomic_compare_exchange64(&alarm_at_msec,
						     alarm_at, ~0UL);
	    if (old == alarm_at)
		kill_thread_siginfo(alarm_target_obj, &si);
	} else {
	    sys_sync_wait(&atomic_read(&alarm_at_msec), alarm_at, alarm_at);
	}
    }
}

unsigned int
alarm(unsigned int seconds)
{
    if (alarm_worker_ct != start_env->proc_container) {
	alarm_target_obj = COBJ(start_env->proc_container, thread_id());
	atomic_set(&alarm_at_msec, ~0UL);

	// Either we forked, or never had an alarm thread to start with.
	int r = thread_create(start_env->proc_container, &alarm_worker, 0,
			      &alarm_worker_obj, "alarm");
	if (r < 0) {
	    cprintf("alarm: cannot create worker thread: %s\n", e2s(r));
	    __set_errno(ENOMEM);
	    return -1;
	}

	alarm_worker_ct = start_env->proc_container;
    }

    uint64_t now = sys_clock_msec();
    uint64_t msec = seconds ? now + seconds * 1000 : ~0UL;
    atomic_set(&alarm_at_msec, msec);
    sys_sync_wakeup(&atomic_read(&alarm_at_msec));
    return 0;
}
