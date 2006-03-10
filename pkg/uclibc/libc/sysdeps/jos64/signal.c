#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <inc/setjmp.h>

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
	thread_halt();
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

int
__sigsetjmp(jmp_buf __env, int __savemask)
{
    jos_setjmp((struct jos_jmp_buf *) __env);
    return 0;
}

void
siglongjmp(sigjmp_buf env, int val)
{
    jos_longjmp((struct jos_jmp_buf *) env, val ? : 1);
}
