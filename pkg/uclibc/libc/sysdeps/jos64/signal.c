#include <machine/trapcodes.h>
#include <machine/x86.h>
#include <machine/pmap.h>
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
#include <inc/jthread.h>

#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>

#include <bits/unimpl.h>
#include <bits/signalgate.h>
#include <bits/kernel_sigaction.h>

enum { signal_debug = 0 };
uint64_t signal_counter;

// BSD compat
const char *sys_signame[_NSIG];

// Thread which will receive traps to handle signals
static uint64_t signal_thread_id;

// Signal handlers
static jthread_mutex_t sigactions_mu;	// don't mask utraps
static struct sigaction sigactions[_NSIG];

// Masked and queued signals
static jthread_mutex_t sigmask_mu;	// mask utraps!
static sigset_t signal_masked;
static sigset_t signal_queued;
static siginfo_t signal_queued_si[_NSIG];
static int signal_queued_any;

static void
utf_dump(struct UTrapframe *utf)
{
    cprintf("rax %016lx  rbx %016lx  rcx %016lx\n",
	    utf->utf_rax, utf->utf_rbx, utf->utf_rcx);
    cprintf("rdx %016lx  rsi %016lx  rdi %016lx\n",
	    utf->utf_rdx, utf->utf_rsi, utf->utf_rdi);
    cprintf("r8  %016lx  r9  %016lx  r10 %016lx\n",
	    utf->utf_r8, utf->utf_r9, utf->utf_r10);
    cprintf("r11 %016lx  r12 %016lx  r13 %016lx\n",
	    utf->utf_r11, utf->utf_r12, utf->utf_r13);
    cprintf("r14 %016lx  r15 %016lx  rbp %016lx\n",
	    utf->utf_r14, utf->utf_r15, utf->utf_rbp);
    cprintf("rip %016lx  rsp %016lx  rflags %016lx\n",
	    utf->utf_rip, utf->utf_rsp, utf->utf_rflags);
}

static int
stack_grow(void *faultaddr)
{
    struct u_segment_mapping usm;
    int r = segment_lookup(faultaddr, &usm);
    if (r < 0)
	return r;
    if (r == 0)
	return -1;

    if (!(usm.flags & SEGMAP_STACK) || !(usm.flags & SEGMAP_REVERSE_PAGES)) {
	// Not a stack?!
	return -1;
    }

    int64_t segbytes = sys_segment_get_nbytes(usm.segment);
    if (segbytes < 0)
	return segbytes;

    // Check if we need to grow anything at all...
    void *stacktop = usm.va + usm.num_pages * PGSIZE;
    uint64_t stackbytes = segbytes - usm.start_page * PGSIZE;
    void *allocbase = stacktop - stackbytes;
    if (faultaddr > stacktop)
	return -1;
    if (faultaddr >= allocbase)
	return 0;

    // Double the stack size, up to the mapping size.
    int64_t newbytes = usm.start_page * PGSIZE +
	(segbytes - usm.start_page * PGSIZE) * 2;
    newbytes = MIN(newbytes, (int64_t) (usm.start_page + usm.num_pages) * PGSIZE);

    // Report no progress if we're already at mapping size.
    if (newbytes < 0 || segbytes == newbytes)
	return 0;

    r = sys_segment_resize(usm.segment, newbytes);
    if (r < 0)
	return r;

    return 1;
}

static void __attribute__((noreturn))
sig_fatal(siginfo_t *si, struct sigcontext *sc)
{
    extern const char *__progname;
    static int fatalities = 0;

    fatalities++;
    if (fatalities > 1) {
	if (fatalities == 2) {
	    cprintf("[%ld] sig_fatal: recursive\n", sys_self_id());
	    print_backtrace(1);
	}

	sys_self_halt();
	cprintf("[%ld] sig_fatal: halt returned\n", sys_self_id());
	for (;;)
	    ;
    }

    switch (si->si_signo) {
    case SIGSEGV: case SIGBUS:  case SIGILL:
	print_backtrace(0);
	segfault_helper(si, sc);
	break;

    case SIGABRT:
	fprintf(stderr, "%s: abort\n", __progname);
	print_backtrace(0);
	break;
    }

    process_exit(0, si->si_signo);
    cprintf("[%ld] sig_fatal: process_exit returned\n", sys_self_id());
    for (;;)
	;
}

static int
signal_trap_thread(struct cobj_ref tobj)
{
    static jthread_mutex_t trap_mu;
    int trap_mu_locked = 0;

    if (thread_id() != signal_thread_id && tobj.object == signal_thread_id) {
	if (jthread_mutex_trylock(&trap_mu) < 0)
	    return 0;
	else
	    trap_mu_locked = 1;
    }

    struct cobj_ref cur_as;
    sys_self_get_as(&cur_as);

    int retry_count = 0;
    for (;;) {
	if (signal_debug)
	    cprintf("[%ld] signal_trap_thread: trying to trap %ld.%ld\n",
		    thread_id(), tobj.container, tobj.object);
	int r = sys_thread_trap(tobj, cur_as, UTRAP_USER_SIGNAL, 0);

	if (r == 0) {
	    if (signal_debug)
		cprintf("[%ld] signal_trap_thread: trapped %ld.%ld\n",
			thread_id(), tobj.container, tobj.object);

	    if (trap_mu_locked)
		jthread_mutex_unlock(&trap_mu);
	    return 0;
	}

	if (r == -E_BUSY) {
	    retry_count++;
	    if (signal_debug || !(retry_count % 10))
		cprintf("[%ld] signal_trap_thread: cannot trap %ld.%ld, retrying\n",
			thread_id(), tobj.container, tobj.object);
	    thread_sleep(10);
	    continue;
	}

	extern const char *__progname;
	cprintf("[%ld] (%s) signal_trap_thread: cannot trap %ld.%ld: %s\n",
		thread_id(), __progname, tobj.container, tobj.object, e2s(r));
	__set_errno(EPERM);
	if (trap_mu_locked)
	    jthread_mutex_unlock(&trap_mu);
	return -1;
    }
}

void
signal_trap_if_pending(void)
{
    if (!signal_queued_any)
	return;

    int pending = 0;
    uint32_t i;

    assert(0 == utrap_set_mask(1));
    jthread_mutex_lock(&sigmask_mu);

    for (i = 0; i < _NSIG; i++) {
	if (sigismember(&signal_queued, i) &&
	    !sigismember(&signal_masked, i))
	{
	    if (signal_debug)
		cprintf("[%ld] signal_trap_if_pending: signal %d\n",
			thread_id(), i);
	    pending++;
	}
    }

    if (pending == 0)
	signal_queued_any = 0;

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(0);

    if (pending)
	signal_trap_thread(COBJ(0, thread_id()));
}

// Called by signal_utrap_onstack() to execute the appropriate signal
// handler for the specified signal.  Already running on the right
// stack for executing the signal handler.
static void
signal_execute(siginfo_t *si, struct sigcontext *sc)
{
    extern const char *__progname;

    jthread_mutex_lock(&sigactions_mu);
    struct sigaction sa = sigactions[si->si_signo];
    jthread_mutex_unlock(&sigactions_mu);

    if (si->si_signo == SIGCHLD) {
	if (signal_debug)
	    cprintf("[%ld] signal_execute: calling child_notify()\n",
		    thread_id());
	child_notify();
    }

    if (sa.sa_handler == SIG_IGN)
	return;

    if (sa.sa_handler == SIG_DFL) {
	switch (si->si_signo) {
	case SIGHUP:  case SIGINT:  case SIGQUIT: case SIGSTKFLT:
	case SIGTRAP: case SIGABRT: case SIGFPE:  case SIGSYS:
	case SIGKILL: case SIGPIPE: case SIGXCPU: case SIGXFSZ:
	case SIGALRM: case SIGTERM: case SIGUSR1: case SIGUSR2:
	case SIGSEGV: case SIGBUS:  case SIGILL:
	    sig_fatal(si, sc);

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

    if (signal_debug)
	cprintf("[%ld] signal_execute: running sigaction\n", thread_id());
    sa.sa_sigaction(si->si_signo, si, sc);
}

// Execute signal handler for si.
// Called with utrap unmasked and no locks held.
// Signal is masked.
// Jumps back to sc to return.
static void __attribute__((noreturn))
signal_utrap_onstack(siginfo_t *si, struct sigcontext *sc)
{
    int signo = si->si_signo;
    if (signal_debug)
	cprintf("[%ld] signal_utrap_onstack: signal %d\n",
		thread_id(), signo);

    if (signo == SIGSEGV && tls_pgfault_all && *tls_pgfault_all) {
	if (signal_debug)
	    cprintf("[%ld] signal_utrap_onstack: longjmp to tls_pgfault_all\n",
		    thread_id());

	utrap_set_mask(1);
	jthread_mutex_lock(&sigmask_mu);
	sigdelset(&signal_masked, signo);
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);

	jos_longjmp(*tls_pgfault_all, 1);
    }


    if (debug_gate_trace() && si->si_signo != SIGKILL) {
	utrap_set_mask(1);
	jthread_mutex_lock(&sigmask_mu);
	sigdelset(&signal_masked, signo);
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);

	debug_gate_on_signal(si->si_signo, sc);
    } else {
	signal_execute(si, sc);

	utrap_set_mask(1);
	jthread_mutex_lock(&sigmask_mu);
	sigdelset(&signal_masked, signo);
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);
    }

    signal_trap_if_pending();

    // resume where we got interrupted; clears utrap
    if (signal_debug)
	cprintf("[%ld] signal_utrap_onstack: returning\n",
		thread_id());

    utrap_ret(&sc->sc_utf);
}

static void
signal_utrap_si(siginfo_t *si, struct sigcontext *sc)
{
    if (signal_debug)
	cprintf("[%ld] signal_utrap_si: signal %d\n",
		thread_id(), si->si_signo);

    // Make sure sigsuspend() wakes up
    signal_counter++;
    sys_sync_wakeup(&signal_counter);

    // Run signal_utrap_onstack(), which will figure out the right
    // signal handler and execute it.
    if (sc->sc_utf.utf_rsp <= (uint64_t) tls_stack_top &&
	sc->sc_utf.utf_rsp > (uint64_t) tls_base)
    {
	// If the trapped stack was the TLS, just call the function
	// to deliver signals, as we're already on the TLS.
	if (signal_debug)
	    cprintf("[%ld] signal_utrap_si: staying on tls stack\n",
		    thread_id());

	utrap_set_mask(0);
	signal_utrap_onstack(si, sc);
    } else {
	// Push a copy of sc, si onto the trapped stack, plus the red zone.
	struct {
	    struct sigcontext sc;
	    siginfo_t si;
	    uint8_t redzone[128];
	} *s;

	s = (void *) sc->sc_utf.utf_rsp;
	s--;

	// Ensure there's space, because we're masking traps right now..
	if (signal_debug)
	    cprintf("[%ld] signal_utrap_si: pre-allocating stack at %p\n",
		    thread_id(), s);

	int r = stack_grow(s);
	if (r < 0) {
	    cprintf("[%ld] signal_utrap_si: stack overflow @ %p\n",
		    thread_id(), s);
	    utf_dump(&sc->sc_utf);

	    si->si_signo = SIGSEGV;
	    si->si_code = SEGV_MAPERR;
	    si->si_addr = s;
	    sig_fatal(si, sc);
	}

	memcpy(&s->si, si, sizeof(*si));
	memcpy(&s->sc, sc, sizeof(*sc));

	if (signal_debug)
	    cprintf("[%ld] signal_utrap_si: jumping to real stack at %p\n",
		    thread_id(), s);

	// jump over there and unmask utrap atomically
	struct UTrapframe utf_jump;
	utf_jump.utf_rflags = read_rflags();
	utf_jump.utf_rip = (uint64_t) &utrap_chain_dwarf2;
	utf_jump.utf_rsp = (uint64_t) s;
	utf_jump.utf_rdi = (uint64_t) &s->si;
	utf_jump.utf_rsi = (uint64_t) &s->sc;

	utf_jump.utf_r15 = (uint64_t) &signal_utrap_onstack;
	utrap_ret(&utf_jump);
    }
}

static void
signal_utrap(struct UTrapframe *utf)
{
    if (signal_debug)
	cprintf("[%ld] signal_utrap: rsp=0x%lx rip=0x%lx\n",
		thread_id(), utf->utf_rsp, utf->utf_rip);

    siginfo_t si;
    memset(&si, 0, sizeof(si));
    int sigmu_taken = 0;

    if (utf->utf_trap_src == UTRAP_SRC_HW) {
	si.si_addr = (void *) utf->utf_trap_arg;
	if (utf->utf_trap_num == T_PGFLT) {
	    int r = stack_grow(si.si_addr);
	    if (r > 0) {
		if (signal_debug)
		    cprintf("[%ld] signal_utrap: grew stack\n", thread_id());
		return;
	    }

	    if (r == 0) {
		if (signal_debug)
		    cprintf("[%ld] signal_utrap: stack_grow returns 0\n",
			    thread_id());
	    }

	    si.si_signo = SIGSEGV;
	    si.si_code = SEGV_ACCERR;	// maybe use segment_lookup()
	} else if (utf->utf_trap_num == T_DEVICE) {
	    int r = sys_self_fp_enable();
	    if (r >= 0) {
		if (signal_debug)
		    cprintf("[%ld] signal_utrap: enabled FP\n", thread_id());
		return;
	    }

	    cprintf("[%ld] signal_utrap: cannot enable fp: %s\n",
		    thread_id(), e2s(r));
	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	} else if (utf->utf_trap_num == T_BRKPT ||
		   utf->utf_trap_num == T_DEBUG) {
	    si.si_signo = SIGTRAP;
	    // XXX TRAP_BRKPT or TRAP_TRACE?
	    si.si_code = TRAP_BRKPT;
	} else {
	    cprintf("[%ld] signal_utrap: unknown hw trap %d\n", thread_id(),
		    utf->utf_trap_num);
	    utf_dump(utf);

	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	}
    } else if (utf->utf_trap_src == UTRAP_SRC_USER) {
	if (utf->utf_trap_num == UTRAP_USER_NOP)
	    return;

	if (utf->utf_trap_num != UTRAP_USER_SIGNAL) {
	    cprintf("[%ld] signal_utrap: unknown user trap# %d\n",
		    thread_id(), utf->utf_trap_num);
	    return;
	}

	jthread_mutex_lock(&sigmask_mu);
	sigmu_taken = 1;

	uint32_t i;
	for (i = 0; i < _NSIG; i++) {
	    if (sigismember(&signal_queued, i) &&
		!sigismember(&signal_masked, i))
	    {
		sigdelset(&signal_queued, i);
		si = signal_queued_si[i];
		assert(si.si_signo == (int) i);
		break;
	    }
	}

	if (i == _NSIG) {
	    // No available signal, just return..
	    jthread_mutex_unlock(&sigmask_mu);
	    if (signal_debug)
		cprintf("[%ld] signal_utrap: no pending signals\n",
			thread_id());
	    return;
	}
    } else {
	cprintf("[%ld] signal_utrap: unknown trap src %d\n",
		thread_id(), utf->utf_trap_src);
	si.si_signo = SIGILL;
	si.si_code = ILL_ILLTRP;
    }

    if (!sigmu_taken)
	jthread_mutex_lock(&sigmask_mu);
    sigaddset(&signal_masked, si.si_signo);
    jthread_mutex_unlock(&sigmask_mu);

    signal_utrap_si(&si, (struct sigcontext *) utf);
}

int
kill_thread_siginfo(struct cobj_ref tobj, siginfo_t *si)
{
    if (si->si_signo < 0 || si->si_signo >= _NSIG) {
	__set_errno(EINVAL);
	return -1;
    }

    int oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);

    if (!sigismember(&signal_queued, si->si_signo)) {
	sigaddset(&signal_queued, si->si_signo);
	memcpy(&signal_queued_si[si->si_signo], si, sizeof(*si));
	signal_queued_any = 1;
    }

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    // Trap the signal-processing thread so it runs the signal handler
    return signal_trap_thread(tobj);
}

void
signal_gate_incoming(siginfo_t *si)
{
    if (signal_debug)
	cprintf("[%ld] signal_gate_incoming: signal %d\n",
		thread_id(), si->si_signo);

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
    uint32_t i;
    int oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);

    if (oldset)
	memcpy(oldset, &signal_masked, sizeof(signal_masked));

    if (set) {
	for (i = 0; i < _SIGSET_NWORDS; i++) {
	    if (how == SIG_SETMASK)
		signal_masked.__val[i] = set->__val[i];
	    else if (how == SIG_BLOCK)
		signal_masked.__val[i] |= set->__val[i];
	    else if (how == SIG_UNBLOCK)
		signal_masked.__val[i] &= ~set->__val[i];
	    else {
		jthread_mutex_unlock(&sigmask_mu);
		utrap_set_mask(oumask);
		__set_errno(EINVAL);
		return -1;
	    }
	}
    }

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    signal_trap_if_pending();
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

    jthread_mutex_lock(&sigactions_mu);

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

    jthread_mutex_unlock(&sigactions_mu);
    return 0;
}

int
sigsuspend(const sigset_t *mask) __THROW
{
    int oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);

    uint64_t ctr = signal_counter;
    if (signal_debug)
	cprintf("[%ld] sigsuspend: starting, ctr=%ld\n",
		thread_id(), ctr);

    sigset_t osigmask;
    memcpy(&osigmask, &signal_masked, sizeof(signal_masked));
    memcpy(&signal_masked, mask, sizeof(signal_masked));

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    signal_trap_if_pending();
    while (signal_counter == ctr) {
	if (signal_debug)
	    cprintf("[%ld] sigsuspend: waiting..\n", thread_id());
	sys_sync_wait(&signal_counter, ctr, ~0ULL);
    }

    oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);
    memcpy(&signal_masked, &osigmask, sizeof(signal_masked));
    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    signal_trap_if_pending();

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
    if (signal_debug)
	cprintf("[%ld] kill_siginfo: pid %ld signal %d\n",
		thread_id(), pid, si->si_signo);

    if (pid == 0) {
	set_enosys();
	return -1;
    }

    if (pid < 0) {
	set_enosys();
	return -1;
    }

    if (pid == getpid()) {
	struct cobj_ref tobj = COBJ(start_env->proc_container, signal_thread_id);
	return kill_thread_siginfo(tobj, si);
    }

    // Send the signal to another process
    uint64_t ct = pid;
    int64_t gate_id = container_find(ct, kobj_gate, "signal");
    if (gate_id < 0) {
	if (signal_debug)
	    cprintf("[%ld] kill_siginfo: cannot find signal gate in %ld: %s\n",
		    thread_id(), ct, e2s(gate_id));
	__set_errno(ESRCH);
	return -1;
    }

    return signal_gate_send(COBJ(ct, gate_id), si);
}

int
__sigsetjmp(sigjmp_buf env, int savemask) __THROW
{
    if (savemask) {
	env->__mask_was_saved = 1;
	int oumask = utrap_set_mask(1);
	jthread_mutex_lock(&sigmask_mu);
	memcpy(&env->__saved_mask, &signal_masked, sizeof(signal_masked));
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(oumask);
    } else {
	env->__mask_was_saved = 0;
    }

    return jos_setjmp(&env->__jmpbuf);
}

void
siglongjmp(sigjmp_buf env, int val) __THROW
{
    if (env->__mask_was_saved) {
	assert(0 == utrap_set_mask(1));
	jthread_mutex_lock(&sigmask_mu);
	memcpy(&signal_masked, &env->__saved_mask, sizeof(signal_masked));
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);

	signal_trap_if_pending();
    }

    jos_longjmp(&env->__jmpbuf, val);
}

int
_setjmp(jmp_buf env)
{
    env->__mask_was_saved = 0;
    return jos_setjmp(&env->__jmpbuf);
}

void
longjmp(jmp_buf env, int val)
{
    jos_longjmp(&env->__jmpbuf, val);
}
