#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <inc/setjmp.h>

#include <bits/unimpl.h>

// BSD compat
const char *sys_signame[_NSIG];

int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    __set_errno(ENOSYS);
    //set_enosys();
    return -1;
}

int
__syscall_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    __set_errno(ENOSYS);
    //set_enosys();
    return -1;
}

int
sigsuspend(const sigset_t *mask)
{
    set_enosys();
    return -1;
}

int
kill(pid_t pid, int sig)
{
    uint64_t self = thread_id();
    if (pid == self) {
	cprintf("kill(): signal %d for self\n", sig);
	thread_halt();
    }

    set_enosys();
    return -1;
}

int
__sigsetjmp(jmp_buf __env, int __savemask)
{
    return jos_setjmp((struct jos_jmp_buf *) __env);
}

void
siglongjmp(sigjmp_buf env, int val)
{
    jos_longjmp((struct jos_jmp_buf *) env, val ? : 1);
}

__sighandler_t
signal(int signum, __sighandler_t handler)
{
    set_enosys();
    return SIG_ERR;
}
