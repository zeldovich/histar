#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <machine/trapcodes.h>

#include <bits/unimpl.h>

// BSD compat
const char *sys_signame[_NSIG];

// Signal handlers
static struct sigaction sigactions[_NSIG];
static siginfo_t siginfos[_NSIG];

// Trap handler to invoke signals
static void
signal_dispatch_sa(struct sigaction *sa, siginfo_t *si)
{
    if (sa->sa_handler == SIG_IGN)
	return;

    if (sa->sa_handler == SIG_DFL) {
	switch (si->si_signo) {
	case SIGHUP:  case SIGINT:  case SIGQUIT: case SIGILL:
	case SIGTRAP: case SIGABRT: case SIGFPE:  case SIGSYS:
	case SIGKILL: case SIGBUS:  case SIGSEGV: case SIGPIPE:
	case SIGALRM: case SIGTERM: case SIGUSR1: case SIGUSR2:
	case SIGXCPU: case SIGXFSZ: case SIGSTKFLT:
	    cprintf("signal_dispatch: fatal signal %d\n", si->si_signo);
	    exit(-1);

	case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
	    cprintf("signal_dispatch: should stop process: %d\n", si->si_signo);
	    return;

	case SIGURG:  case SIGCONT: case SIGCHLD: case SIGWINCH:
	case SIGINFO:
	    return;

	default:
	    cprintf("signal_dispatch: unhandled default %d\n", si->si_signo);
	    exit(-1);
	}
    }

    sa->sa_sigaction(si->si_signo, si, 0);
}

static void
signal_dispatch(siginfo_t *si)
{
    struct sigaction *sa = &sigactions[si->si_signo];

    // XXX check if the signal is masked right now; return if so.

    // XXX save current sigmask; mask the signal and sa->sa_mask

    signal_dispatch_sa(sa, si);

    // XXX restore saved sigmask
}

static void
signal_utrap(struct UTrapframe *utf)
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));

    if (utf->utf_trap_src == UTRAP_SRC_HW) {
	si.si_addr = (void *) utf->utf_trap_arg;
	if (utf->utf_trap_num == T_PGFLT) {
	    si.si_signo = SIGSEGV;
	    si.si_code = SEGV_ACCERR;	// maybe use segment_lookup()
	} else {
	    cprintf("signal_utrap: unknown hw trap %d\n", utf->utf_trap_num);
	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	}
    } else if (utf->utf_trap_src == UTRAP_SRC_USER) {
	uint64_t signo = utf->utf_trap_arg;
	if (signo >= _NSIG) {
	    cprintf("signal_utrap: bad user signal %lu\n", signo);
	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	} else {
	    si = siginfos[signo];
	}
    } else {
	cprintf("signal_utrap: unknown trap src %d\n", utf->utf_trap_src);
	si.si_signo = SIGILL;
	si.si_code = ILL_ILLTRP;
    }

    signal_dispatch(&si);
}

static void
signal_utrap_init(void)
{
    static int signal_inited;
    if (signal_inited == 0) {
	signal_inited = 1;
	utrap_set_handler(&signal_utrap);
    }
}

// Signal gates
void
signal_gate_create(void)
{
    signal_utrap_init();
    // XXX create a gate to dispatch signals
}

// System calls emulated in jos64
int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    __set_errno(ENOSYS);
    return -1;
}

int
__syscall_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    __set_errno(ENOSYS);
    return -1;
}

int
sigsuspend(const sigset_t *mask)
{
    __set_errno(ENOSYS);
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
