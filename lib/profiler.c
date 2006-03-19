#include <inc/profiler.h>
#include <inc/lib.h>
#include <inc/stdio.h>

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static struct cobj_ref prof_thread;
static struct cobj_ref prof_target;
static int prof_enable;

static void
profiler_sig(int signo, siginfo_t *si, void *arg)
{
    printf("profiler signal\n");
}

static void
profiler_thread(void *arg)
{
    cprintf("profiler_thread\n");
}

static void
profiler_exit(void)
{
    cprintf("profiler_exit\n");
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
