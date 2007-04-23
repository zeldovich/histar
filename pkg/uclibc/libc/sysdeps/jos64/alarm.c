#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/signal.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#include <bits/unimpl.h>

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>

static struct cobj_ref alarm_worker_obj;
static uint64_t alarm_worker_ct;

static struct cobj_ref alarm_target_obj;

static jos_atomic64_t alarm_at_nsec;

static void __attribute__((noreturn))
alarm_worker(void *arg)
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_signo = SIGALRM;

    for (;;) {
	uint64_t now = sys_clock_nsec();
	uint64_t alarm_at = jos_atomic_read(&alarm_at_nsec);

	if (now >= alarm_at) {
	    uint64_t old = jos_atomic_compare_exchange64(&alarm_at_nsec,
							 alarm_at, ~0UL);
	    if (old == alarm_at)
		kill_thread_siginfo(alarm_target_obj, &si);
	} else {
	    sys_sync_wait(&jos_atomic_read(&alarm_at_nsec), alarm_at, alarm_at);
	}
    }
}

unsigned int
alarm(unsigned int seconds)
{
    if (alarm_worker_ct != start_env->proc_container) {
	alarm_target_obj = COBJ(start_env->proc_container, thread_id());
	jos_atomic_set(&alarm_at_nsec, ~0UL);

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

    uint64_t now = sys_clock_nsec();
    uint64_t nsec = seconds ? now + NSEC_PER_SECOND * seconds : ~0UL;
    jos_atomic_set(&alarm_at_nsec, nsec);
    sys_sync_wakeup(&jos_atomic_read(&alarm_at_nsec));
    return 0;
}

int
setitimer(__itimer_which_t which, const struct itimerval *value,
	  struct itimerval *ovalue)
{
    set_enosys();
    return -1;
}
