#include <inc/profiler.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/signal.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

static uint64_t delay_nsec = NSEC_PER_SECOND / 100;
enum { buffer_size = 4096 };

static struct cobj_ref prof_thread;
static struct cobj_ref prof_target;
static int prof_enable;

static uint64_t prof_rip;
static uint64_t prof_samples[buffer_size];
static uint64_t prof_sample_next;

static void
profiler_sig(int signo, siginfo_t *si, void *arg)
{
    struct sigcontext *sc = (struct sigcontext *) arg;
    prof_rip = sc->sc_utf.utf_pc;
    sys_sync_wakeup(&prof_rip);
}

static void
profiler_thread(void *arg)
{
    uint64_t now = sys_clock_nsec();
    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_signo = SIGUSR1;

    while (prof_enable < 2) {
	sys_sync_wait(&now, now, now + delay_nsec);
	now = sys_clock_nsec();

	prof_rip = 0;
	kill_thread_siginfo(prof_target, &si);

	while (!prof_rip)
	    sys_sync_wait(&prof_rip, 0, UINT64(~0));

	prof_samples[prof_sample_next] = prof_rip;
	prof_sample_next = (prof_sample_next + 1) % buffer_size;
    }
}

static void
profiler_exit(void)
{
    prof_enable = 2;

    printf("Profiling samples:");
    for (uint32_t i = 0; i < buffer_size; i++)
	if (prof_samples[i])
	    printf(" %"PRIx64, prof_samples[i]);
    printf("\n");
}

void
profiler_init(void)
{
    if (prof_enable) {
	cprintf("profiler_init: already enabled\n");
	return;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &profiler_sig;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &sa, 0) < 0) {
	cprintf("profiler_init: cannot register SIGUSR1: %s\n", strerror(errno));
	return;
    }

    prof_target = COBJ(start_env->proc_container, thread_id());

    int r = thread_create(start_env->proc_container, &profiler_thread, 0,
			  &prof_thread, "profiler thread");
    if (r < 0) {
	cprintf("profiler_init: cannot create thread: %s\n", e2s(r));
	return;
    }

    if (atexit(&profiler_exit) < 0) {
	cprintf("profiler_init: cannot register atexit: %s\n", strerror(errno));
	return;
    }

    prof_enable = 1;
}
