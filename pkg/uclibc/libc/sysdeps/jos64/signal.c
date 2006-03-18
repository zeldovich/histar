#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <errno.h>
#include <signal.h>
#include <inc/signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <machine/trapcodes.h>

#include <bits/unimpl.h>
#include <bits/signalgate.h>

static int signal_debug = 0;
static uint64_t signal_counter;

// BSD compat
const char *sys_signame[_NSIG];

// Signal handlers
static struct sigaction sigactions[_NSIG];
static siginfo_t siginfos[_NSIG];

// Trap handler to invoke signals
static void
signal_dispatch_sa(struct sigaction *sa, siginfo_t *si)
{
    extern const char *__progname;

    if (sa->sa_handler == SIG_IGN)
	return;

    if (sa->sa_handler == SIG_DFL) {
	switch (si->si_signo) {
	case SIGHUP:  case SIGINT:  case SIGQUIT: case SIGILL:
	case SIGTRAP: case SIGABRT: case SIGFPE:  case SIGSYS:
	case SIGKILL: case SIGBUS:  case SIGSEGV: case SIGPIPE:
	case SIGALRM: case SIGTERM: case SIGUSR1: case SIGUSR2:
	case SIGXCPU: case SIGXFSZ: case SIGSTKFLT:
	    cprintf("%s: fatal signal %d\n", __progname, si->si_signo);
	    exit(-1);

	case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
	    cprintf("%s: should stop process: %d\n", __progname, si->si_signo);
	    return;

	case SIGURG:  case SIGCONT: case SIGCHLD: case SIGWINCH:
	case SIGINFO:
	    return;

	default:
	    cprintf("%s: unhandled default signal %d\n", __progname, si->si_signo);
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

    signal_counter++;
    sys_sync_wakeup(&signal_counter);
}

static void
signal_utrap(struct UTrapframe *utf)
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));

    if (utf->utf_trap_src == UTRAP_SRC_HW) {
	si.si_addr = (void *) utf->utf_trap_arg;
	if (utf->utf_trap_num == T_PGFLT) {
	    cprintf("signal_utrap: pagefault, va=0x%lx, rip=0x%lx, rsp=0x%lx\n",
		    utf->utf_trap_arg, utf->utf_rip, utf->utf_rsp);

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

int
kill_thread_siginfo(struct cobj_ref tid, siginfo_t *si)
{
    uint32_t signo = si->si_signo;
    if (signo >= _NSIG) {
	__set_errno(EINVAL);
	return -1;
    }

    siginfos[signo] = *si;
    int r = sys_thread_trap(tid, 0, signo);
    if (r < 0) {
	cprintf("kill_thread_siginfo: cannot trap: %s\n", e2s(r));
	__set_errno(EPERM);
	return -1;
    }

    return 0;
}

void
signal_process_remote(siginfo_t *si)
{
    int64_t id = container_find(start_env->proc_container, kobj_thread, 0);
    if (id >= 0) {
	struct cobj_ref tobj = COBJ(start_env->proc_container, id);
	kill_thread_siginfo(tobj, si);
    }
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

void
signal_init(void)
{
    signal_utrap_init();
    signal_gate_init();
}

// System calls emulated in jos64
int
sigprocmask(int how, const sigset_t *set, sigset_t *oldset) __THROW
{
    __set_errno(ENOSYS);
    return -1;
}

int
__syscall_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    if (signum < 0 || signum >= _NSIG) {
	__set_errno(EINVAL);
	return -1;
    }

    // XXX the memcpy below somehow crashes ksh..  huh?
    if (oldact && 0)
	memcpy(oldact, &sigactions[signum], sizeof(*oldact));
    if (act)
	memcpy(&sigactions[signum], act, sizeof(*act));
    return 0;
}

int
sigsuspend(const sigset_t *mask) __THROW
{
    uint64_t ctr = signal_counter;

    int i;
    for (i = 1; i < _NSIG; i++) {
	if (sigismember(mask, i)) {
	    set_enosys();
	    return -1;
	}
    }

    // Since our signal masking is nonexistant, we fudge with a timeout
    sys_sync_wait(&signal_counter, ctr, sys_clock_msec() + 1000);

    __set_errno(EINTR);
    return -1;
}

int
kill(pid_t pid, int sig) __THROW
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_pid = getpid();
    si.si_signo = sig;
    return kill_siginfo(pid, &si);
}

int
kill_siginfo(pid_t pid, siginfo_t *si)
{
    pid_t self = getpid();

    if (pid == self) {
	signal_dispatch(si);
	return 0;
    }

    uint64_t ct = pid;
    int64_t gate_id = container_find(ct, kobj_gate, "signal");
    if (gate_id < 0) {
	if (signal_debug)
	    cprintf("kill: cannot find signal gate in %ld: %s\n",
		    ct, e2s(gate_id));
	__set_errno(ESRCH);
	return -1;
    }

    return signal_gate_send(COBJ(ct, gate_id), si);
}

int
__sigsetjmp(jmp_buf __env, int __savemask) __THROW
{
    return jos_setjmp((struct jos_jmp_buf *) __env);
}

void
siglongjmp(sigjmp_buf env, int val) __THROW
{
    jos_longjmp((struct jos_jmp_buf *) env, val);
}
