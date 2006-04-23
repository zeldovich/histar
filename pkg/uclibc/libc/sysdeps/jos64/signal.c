#include <machine/trapcodes.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/signal.h>
#include <inc/setjmp.h>
#include <inc/utrap.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/wait.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

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
static uint64_t signal_thread_id;

static void
sig_fatal(void)
{
    static int recursive;

    if (recursive) {
	sys_self_halt();
    } else {
	recursive = 1;

	print_backtrace();
	exit(-1);
    }
}

static void
signal_dispatch_sa(struct sigaction *sa, siginfo_t *si, struct sigcontext *sc)
{
    extern const char *__progname;

    if (si->si_signo == SIGCHLD)
	child_notify();

    if (sa->sa_handler == SIG_IGN)
	return;

    if (sa->sa_handler == SIG_DFL) {
	switch (si->si_signo) {
	case SIGHUP:  case SIGINT:  case SIGQUIT: case SIGSTKFLT:
	case SIGTRAP: case SIGABRT: case SIGFPE:  case SIGSYS:
	case SIGKILL: case SIGPIPE: case SIGXCPU: case SIGXFSZ:
	case SIGALRM: case SIGTERM: case SIGUSR1: case SIGUSR2:
	    cprintf("%s: fatal signal %d\n", __progname, si->si_signo);
	    sig_fatal();
	    break;

	case SIGSEGV: case SIGBUS:  case SIGILL:
	    cprintf("%s: fatal signal %d, addr=%p\n",
		    __progname, si->si_signo, si->si_addr);
	    if (sc)
		cprintf("%s: rip=0x%lx, rsp=0x%lx\n",
			__progname, sc->sc_utf.utf_rip, sc->sc_utf.utf_rsp);
	    segfault_helper(si->si_addr);

	    sig_fatal();
	    break;

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

    sa->sa_sigaction(si->si_signo, si, sc);
}

static void
signal_dispatch(siginfo_t *si, struct sigcontext *sc)
{
    struct sigaction *sa = &sigactions[si->si_signo];

    // XXX check if the signal is masked right now; return if so.

    // XXX save current sigmask; mask the signal and sa->sa_mask

    signal_dispatch_sa(sa, si, sc);

    // XXX restore saved sigmask

    signal_counter++;
    sys_sync_wakeup(&signal_counter);
}

static int
stack_grow(void *faultaddr)
{
    void *stacktop = (void *) USTACKTOP;
    void *stackbase = (void *) USTACKBASE;
    if (faultaddr >= stacktop || faultaddr < stackbase)
	return -1;

    uint32_t cur_pages = 0;
    while (stacktop - cur_pages * PGSIZE >= stackbase) {
	uint32_t check_pages = cur_pages + 1;
	int r = segment_lookup(stacktop - check_pages * PGSIZE, 0, 0, 0);
	if (r < 0)
	    return r;
	if (r == 0)
	    break;
	cur_pages = check_pages;
    }

    // If we are faulting on allocated stack, something is wrong.
    if (faultaddr >= stacktop - cur_pages * PGSIZE)
	return -1;

    // Double our stack size
    struct cobj_ref new_stack_seg;
    void *new_va = stacktop - cur_pages * PGSIZE * 2;
    int r = segment_alloc(start_env->proc_container,
			  cur_pages * PGSIZE, &new_stack_seg,
			  &new_va, 0, "stack growth");
    if (r < 0)
	return r;

    return 0;
}

static void
signal_utrap(struct UTrapframe *utf)
{
    siginfo_t si;
    memset(&si, 0, sizeof(si));

    if (utf->utf_trap_src == UTRAP_SRC_HW) {
	si.si_addr = (void *) utf->utf_trap_arg;
	if (utf->utf_trap_num == T_PGFLT) {
	    if (stack_grow(si.si_addr) >= 0)
		return;

	    si.si_signo = SIGSEGV;
	    si.si_code = SEGV_ACCERR;	// maybe use segment_lookup()
	} else if (utf->utf_trap_num == T_DEVICE) {
	    int r = sys_self_fp_enable();
	    if (r >= 0) {
		//cprintf("signal_utrap: enabled floating-point\n");
		return;
	    }

	    cprintf("signal_utrap: cannot enable fp: %s\n", e2s(r));
	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	} else {
	    cprintf("signal_utrap: unknown hw trap %d\n", utf->utf_trap_num);

	    cprintf("signal_utrap: rax %016lx  rbx %016lx  rcx %016lx\n",
		    utf->utf_rax, utf->utf_rbx, utf->utf_rcx);
	    cprintf("signal_utrap: rdx %016lx  rsi %016lx  rdi %016lx\n",
		    utf->utf_rdx, utf->utf_rsi, utf->utf_rdi);
	    cprintf("signal_utrap: r8  %016lx  r9  %016lx  r10 %016lx\n",
		    utf->utf_r8, utf->utf_r9, utf->utf_r10);
	    cprintf("signal_utrap: r11 %016lx  r12 %016lx  r13 %016lx\n",
		    utf->utf_r11, utf->utf_r12, utf->utf_r13);
	    cprintf("signal_utrap: r14 %016lx  r15 %016lx  rbp %016lx\n",
		    utf->utf_r14, utf->utf_r15, utf->utf_rbp);
	    cprintf("signal_utrap: rip %016lx  rsp %016lx  rflags %016lx\n",
		    utf->utf_rip, utf->utf_rsp, utf->utf_rflags);

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

    signal_dispatch(&si, (struct sigcontext *) utf);
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

    struct cobj_ref cur_as;
    sys_self_get_as(&cur_as);
    int r = sys_thread_trap(tid, cur_as, 0, signo);
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
    struct cobj_ref tobj = COBJ(start_env->proc_container, signal_thread_id);
    kill_thread_siginfo(tobj, si);
}

static void
signal_utrap_init(void)
{
    static int signal_inited;
    if (signal_inited == 0) {
	signal_inited = 1;
	signal_thread_id = thread_id();
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
    // Fake it; sleep in particular refuses to sleep if we return error
    if (oldset)
	__sigemptyset(oldset);

    return 0;
}

// Fake prototype to make GCC happy.
int __syscall_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

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
	signal_dispatch(si, 0);
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
__sigsetjmp(jmp_buf env, int savemask) __THROW
{
    return jos_setjmp((struct jos_jmp_buf *) env);
}

void
siglongjmp(sigjmp_buf env, int val) __THROW
{
    jos_longjmp((struct jos_jmp_buf *) env, val);
}

int
_setjmp(jmp_buf env)
{
    return jos_setjmp((struct jos_jmp_buf *) env);
}

void
longjmp(jmp_buf env, int val)
{
    jos_longjmp((struct jos_jmp_buf *) env, val);
}
