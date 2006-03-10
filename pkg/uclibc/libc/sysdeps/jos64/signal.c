#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

// BSD compat
const char *sys_signame[_NSIG];

int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    __set_errno(ENOSYS);
    return -1;
}

int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    __set_errno(ENOSYS);
    return -1;
}

int
raise(int sig)
{
    if (sig == SIGABRT) {
	cprintf("raising SIGABRT\n");
	process_report_exit(-SIGABRT);
	return sys_thread_halt();
    }

    cprintf("raise(%d)\n", sig);
    _exit(-sig);
}

int
kill(pid_t pid, int sig)
{
    __set_errno(ENOSYS);
    return -1;
}

int
killpg(int pgrp, int sig)
{
    __set_errno(ENOSYS);
    return -1;
}
