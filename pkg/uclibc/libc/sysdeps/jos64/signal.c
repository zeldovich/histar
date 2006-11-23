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
#include <inc/debug_gate.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include <bits/unimpl.h>
#include <bits/signalgate.h>
#include <bits/kernel_sigaction.h>

static int signal_debug = 0;
uint64_t signal_counter;

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
	    segfault_helper(si, sc);
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

    if (debug_gate_trace() && si->si_signo != SIGKILL)
	debug_gate_on_signal(si->si_signo, sc);
    else
	signal_dispatch_sa(sa, si, sc);

    // XXX restore saved sigmask

    signal_counter++;
    sys_sync_wakeup(&signal_counter);
}

static int
stack_grow(void *faultaddr)
{    
    struct u_segment_mapping usm;
    int r = segment_lookup(faultaddr, &usm);
    if (r < 0)
	return r;
    if (r == 0 || !(usm.flags & SEGMAP_STACK))
	return -1;

    uint64_t max_pages = usm.num_pages;
    void *stacktop = usm.va + max_pages * PGSIZE;

    struct cobj_ref stack_seg;
    uint64_t stack_seg_npages = 0;
    uint64_t mapped_pages = 0;

    while (mapped_pages < max_pages) {
	uint64_t check_pages = mapped_pages + 1;
	r = segment_lookup_skip(stacktop - check_pages * PGSIZE,
				&usm, SEGMAP_RESERVE);
	if (r < 0)
	    return r;
	if (r == 0)
	    break;

	if (mapped_pages == 0)
	    stack_seg = usm.segment;
	if (usm.segment.object == stack_seg.object)
	    stack_seg_npages = MAX(stack_seg_npages,
				   usm.start_page + usm.num_pages);

	mapped_pages = check_pages;
    }

    // If we are faulting on allocated stack, something is wrong.
    if (faultaddr >= stacktop - mapped_pages * PGSIZE)
	return -1;

    // If we have no stack, something is wrong too.
    if (mapped_pages == 0)
	return -1;

    // Double our stack size
    uint64_t new_pages = MIN(max_pages - mapped_pages, mapped_pages);
    r = sys_segment_resize(stack_seg, (stack_seg_npages + new_pages) * PGSIZE);
    if (r < 0)
	return r;

    void *new_va = stacktop - mapped_pages * PGSIZE - new_pages * PGSIZE;
    uint64_t map_bytes = new_pages * PGSIZE;
    r = segment_map(stack_seg, stack_seg_npages * PGSIZE,
		    SEGMAP_READ | SEGMAP_WRITE,
		    &new_va, &map_bytes, 0);
    if (r < 0) {
	cprintf("stack_grow: dangling unmapped stack bytes\n");
	return r;
    }

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
	} else if (utf->utf_trap_num == T_BRKPT ||
		   utf->utf_trap_num == T_DEBUG) {
	    si.si_signo = SIGTRAP;
	    // XXX TRAP_BRKPT or TRAP_TRACE?
	    si.si_code = TRAP_BRKPT;
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

    /* XXX implement real signal queuing */
    for (;;) {
	int r = sys_thread_trap(tid, cur_as, 0, signo);

	if (r == 0)
	    break;

	if (r == -E_BUSY) {
	    cprintf("kill_thread_siginfo: cannot trap %ld.%ld, retrying\n",
		    tid.container, tid.object);
	    thread_sleep(10);
	    continue;
	}

	if (r < 0) {
	    cprintf("kill_thread_siginfo: cannot trap: %s\n", e2s(r));
	    __set_errno(EPERM);
	    return -1;
	}
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
    signal_thread_id = thread_id();
    utrap_set_handler(&signal_utrap);
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

int 
__syscall_sigaction (int signum, const struct old_kernel_sigaction *act,
		     struct old_kernel_sigaction *oldact)
{
    if (signum < 0 || signum >= _NSIG) {
	__set_errno(EINVAL);
	return -1;
    }

    if (oldact) {
	oldact->k_sa_handler = sigactions[signum].sa_handler;
	oldact->sa_mask = sigactions[signum].sa_mask.__val[0];
	oldact->sa_flags = sigactions[signum].sa_flags;
	oldact->sa_restorer = sigactions[signum].sa_restorer;
    }

    if (act) {
	sigactions[signum].sa_handler = act->k_sa_handler;
	sigactions[signum].sa_mask.__val[0] = act->sa_mask;
	sigactions[signum].sa_flags = act->sa_flags;
	sigactions[signum].sa_restorer = act->sa_restorer;
    }

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
    if (pid == 0) {
	set_enosys();
	return -1;
    }

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
