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
#include <inttypes.h>

#include <bits/unimpl.h>
#include <bits/signalgate.h>
#include <bits/kernel_sigaction.h>

#if defined(JOS_ARCH_amd64) || defined(JOS_ARCH_i386) || defined (JOS_ARCH_nacl)
 #include <machine/x86.h>
 #include <machine/trapcodes.h>
 #define ARCH_PGFLTD  T_PGFLT
 #define ARCH_PGFLTI  T_PGFLT
 #define ARCH_DEVICE  T_DEVICE
 #define ARCH_BRKPT   T_BRKPT
 #define ARCH_DEBUG   T_DEBUG
 #define ARCH_DIVIDE  T_DIVIDE

 #define JOS_ONSTACK_GCCATTR regparm(2)
#elif defined(JOS_ARCH_sparc)
 #include <machine/trapcodes.h>
 #define ARCH_PGFLTD  T_DATAFAULT
 #define ARCH_PGFLTI  T_TEXTFAULT
 #define ARCH_DEVICE  T_FP_DISABLED
 #define ARCH_BRKPT   T_BRKPT
 #define ARCH_DEBUG   T_BRKPT
 #define ARCH_DIVIDE  T_DIVIDE

 #define JOS_ONSTACK_GCCATTR
#else
#error Unknown arch
#endif

enum { signal_debug = 0 };
enum { signal_debug_utf_dump = 0 };
volatile uint64_t signal_counter;

// BSD compat
const char *sys_signame[_NSIG] = {
    [0] = "",
#define SYS_SIGNAME(signame) [signame] = #signame + 3
    SYS_SIGNAME(SIGHUP),
    SYS_SIGNAME(SIGINT),
    SYS_SIGNAME(SIGQUIT),
    SYS_SIGNAME(SIGILL),
    SYS_SIGNAME(SIGTRAP),
    SYS_SIGNAME(SIGABRT),
    SYS_SIGNAME(SIGBUS),
    SYS_SIGNAME(SIGFPE),
    SYS_SIGNAME(SIGKILL),
    SYS_SIGNAME(SIGUSR1),
    SYS_SIGNAME(SIGSEGV),
    SYS_SIGNAME(SIGUSR2),
    SYS_SIGNAME(SIGPIPE),
    SYS_SIGNAME(SIGALRM),
    SYS_SIGNAME(SIGTERM),
    SYS_SIGNAME(SIGSTKFLT),
    SYS_SIGNAME(SIGCHLD),
    SYS_SIGNAME(SIGCONT),
    SYS_SIGNAME(SIGSTOP),
    SYS_SIGNAME(SIGTSTP),
    SYS_SIGNAME(SIGTTIN),
    SYS_SIGNAME(SIGTTOU),
    SYS_SIGNAME(SIGURG),
    SYS_SIGNAME(SIGXCPU),
    SYS_SIGNAME(SIGXFSZ),
    SYS_SIGNAME(SIGVTALRM),
    SYS_SIGNAME(SIGPROF),
    SYS_SIGNAME(SIGWINCH),
    SYS_SIGNAME(SIGIO),
    SYS_SIGNAME(SIGPWR),
    SYS_SIGNAME(SIGSYS),
    SYS_SIGNAME(SIGINFO),
#undef SYS_SIGNAME
    [SIGINFO + 1 ... _NSIG - 1] = "",
};

// Thread which will receive traps to handle signals
static uint64_t signal_thread_id;
static int signal_did_shutdown;		// _exit() called

// Signal handlers
static jthread_mutex_t sigactions_mu;	// don't mask utraps
static struct sigaction sigactions[_NSIG];

// Masked and queued signals
static jthread_mutex_t sigmask_mu;	// mask utraps!
static sigset_t signal_masked;
static sigset_t signal_queued;
static siginfo_t signal_queued_si[_NSIG];

// Avoid growing stack in signal handler, for vmlinux
int signal_grow_stack;

libc_hidden_proto(sigprocmask)
libc_hidden_proto(sigsuspend)
libc_hidden_proto(sigwaitinfo)
libc_hidden_proto(kill)

static void
utf_dump(struct UTrapframe *utf)
{
#if defined(JOS_ARCH_amd64)
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
#elif defined(JOS_ARCH_i386) || defined(JOS_ARCH_nacl)
    cprintf("eax %08x  ebx %08x  ecx %08x  edx %08x\n",
            utf->utf_eax, utf->utf_ebx, utf->utf_ecx, utf->utf_edx);
    cprintf("esi %08x  edi %08x  ebp %08x  esp %08x\n",
            utf->utf_esi, utf->utf_edi, utf->utf_ebp, utf->utf_esp);
    cprintf("eip %08x  eflags %08x\n",
            utf->utf_eip, utf->utf_eflags);
#elif defined(JOS_ARCH_sparc)
    cprintf("       globals     outs   locals      ins\n");
    for (uint32_t i = 0; i < 8; i++) {
	cprintf("   %d: %08x %08x %08x %08x\n", 
		i, utf->utf_reg[i], utf->utf_reg[i + 8],
		utf->utf_reg[i + 16], utf->utf_reg[i + 24]);
    }
    cprintf("\n");
    cprintf(" y: %08x  pc: %08x  npc: %08x\n",
	    utf->utf_y, utf->utf_pc, utf->utf_npc);
#else
#error Unknown arch
#endif
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
	// Not a stack.

	if (usm.flags & SEGMAP_VECTOR_PF) {
	    assert(tls_data->tls_pgfault);
	    utrap_set_mask(0);
	    jos_longjmp(tls_data->tls_pgfault, 1);
	}

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
    static int fatalities = 0;

    fatalities++;
    if (fatalities > 1) {
	if (fatalities == 2) {
	    cprintf("[%"PRIu64"] %s: sig_fatal: recursive\n",
		    sys_self_id(), jos_progname);
	    print_backtrace(1);
	}

	sys_self_halt();
	cprintf("[%"PRIu64"] %s: sig_fatal: halt returned\n",
		sys_self_id(), jos_progname);
	for (;;)
	    ;
    }

    switch (si->si_signo) {
    case SIGSEGV: case SIGBUS:  case SIGILL:
	fprintf(stderr, "[%"PRIu64"] %s: fatal signal %d "
			"(rip=0x%zx, rsp=0x%zx), backtrace follows.\n",
		sys_self_id(), jos_progname, si->si_signo,
		sc->sc_utf.utf_pc, sc->sc_utf.utf_stackptr);
	print_backtrace(0);
	segfault_helper(si, sc);
	break;

    case SIGABRT:
	fprintf(stderr, "[%"PRIu64"] %s: abort\n", sys_self_id(), jos_progname);
	print_backtrace(0);
	break;

    default:
	break;
    }

    process_exit(0, si->si_signo);
    cprintf("[%"PRIu64"] %s: sig_fatal: process_exit returned\n",
	    sys_self_id(), jos_progname);
    for (;;)
	;
}

void
signal_shutdown(void)
{
    signal_did_shutdown = 1;
}

static int
signal_trap_thread(struct cobj_ref tobj, int signo)
{
    static jthread_mutex_t trap_mu;
    int trap_mu_locked = 0;

    static jos_atomic_t trap_count;
    uint32_t my_trap_count;

    for (;;) {
	uint32_t cur_count = jos_atomic_read(&trap_count);
	my_trap_count = cur_count + 1;
	if (jos_atomic_compare_exchange(&trap_count, cur_count,
					my_trap_count) == cur_count)
	    break;
    }

 retry_mu:
    if (thread_id() != signal_thread_id && tobj.object == signal_thread_id) {
	if (jthread_mutex_trylock(&trap_mu) < 0) {
	    if (signal_debug)
		cprintf("[%"PRIu64"] signal_trap_thread: lock busy\n",
			thread_id());
	    return 0;
	} else {
	    trap_mu_locked = 1;
	}
    }

    struct cobj_ref cur_as;
    sys_self_get_as(&cur_as);

    int retry_warn = 16;
    int retry_count = 0;
    int rv = 0;
    for (;;) {
	if (signal_did_shutdown) {
	    if (signal_debug)
		cprintf("[%"PRIu64"] signal_trap_thread: pid %"PRIu64" exited "
			"(sig=%d)\n",
			thread_id(), start_env->shared_container, signo);
	    __set_errno(ESRCH);
	    return -1;
	}

	if (signal_debug)
	    cprintf("[%"PRIu64"] signal_trap_thread: "
		    "trying to trap %"PRIu64".%"PRIu64"\n",
		    thread_id(), tobj.container, tobj.object);
	int r = sys_thread_trap(tobj, cur_as, UTRAP_USER_SIGNAL, 0);

	if (r == 0) {
	    if (signal_debug)
		cprintf("[%"PRIu64"] signal_trap_thread: "
			"trapped %"PRIu64".%"PRIu64"\n",
			thread_id(), tobj.container, tobj.object);

	    if (trap_mu_locked)
		jthread_mutex_unlock(&trap_mu);

	    rv = 0;
	    goto done;
	}

	if (r == -E_BUSY) {
	    retry_count++;
	    if (signal_debug || retry_count > retry_warn) {
		cprintf("[%"PRIu64"] (%s.%"PRIu64") signal_trap_thread: "
			"cannot trap %"PRIu64".%"PRIu64", retrying (sig=%d)\n",
			thread_id(), jos_progname,
			start_env->shared_container,
			tobj.container, tobj.object, signo);
		retry_warn <<= 1;
	    }
	    thread_sleep_nsec(NSEC_PER_SECOND / 100);
	    continue;
	}

	/* Check if the process is actually alive */
	struct process_state *ps = 0;
	int mr = segment_map(start_env->process_status_seg,
			     0, SEGMAP_READ, (void **) &ps, 0, 0);
	if (mr >= 0) {
	    uint64_t status = ps->status;
	    segment_unmap(ps);

	    if (status == PROCESS_EXITED) {
		__set_errno(ESRCH);
		goto err;
	    }
	}

	cprintf("[%"PRIu64"] (%s) signal_trap_thread: "
		"cannot trap %"PRIu64".%"PRIu64": %s\n",
		thread_id(), jos_progname, tobj.container, tobj.object, e2s(r));
	__set_errno(EPERM);
 err:
	if (trap_mu_locked)
	    jthread_mutex_unlock(&trap_mu);

	rv = -1;
	goto done;
    }

 done:
    if (jos_atomic_read(&trap_count) != my_trap_count) {
	my_trap_count = jos_atomic_read(&trap_count);
	goto retry_mu;
    }

    return rv;
}

void
signal_trap_if_pending(void)
{
    sigset_t ready;
    for (uint32_t w = 0; w < _SIGSET_NWORDS; w++)
	ready.__val[w] = signal_queued.__val[w] & ~signal_masked.__val[w];
    if (__sigisemptyset(&ready))
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
		cprintf("[%"PRIu64"] signal_trap_if_pending: signal %d\n",
			thread_id(), i);
	    pending++;
	}
    }

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(0);

    if (pending)
	signal_trap_thread(COBJ(0, thread_id()), 0);
}

// Called by signal_utrap_onstack() to execute the appropriate signal
// handler for the specified signal.  Already running on the right
// stack for executing the signal handler.
static void
signal_execute(siginfo_t *si, struct sigcontext *sc)
{
    jthread_mutex_lock(&sigactions_mu);
    struct sigaction sa = sigactions[si->si_signo];
    jthread_mutex_unlock(&sigactions_mu);

    if (si->si_signo == SIGINFO) {
	fprintf(stderr,
		"%s (pid %"PRIu64", tid %"PRIu64"): SIGINFO backtrace..\n",
		jos_progname, start_env->shared_container, thread_id());
	print_backtrace(0);
    }

    if (si->si_signo == SIGCHLD) {
	if (signal_debug)
	    cprintf("[%"PRIu64"] signal_execute: calling child_notify()\n",
		    thread_id());
	child_notify();
    }

    if (sa.sa_handler == SIG_IGN)
	return;

    if (sa.sa_handler == SIG_DFL) {
        struct process_state *ps = 0;
        int r = 0;

	switch (si->si_signo) {
	case SIGHUP:  case SIGINT:  case SIGQUIT: case SIGSTKFLT:
	case SIGTRAP: case SIGABRT: case SIGFPE:  case SIGSYS:
	case SIGKILL: case SIGPIPE: case SIGXCPU: case SIGXFSZ:
	case SIGALRM: case SIGTERM: case SIGUSR1: case SIGUSR2:
	case SIGSEGV: case SIGBUS:  case SIGILL:
	    sig_fatal(si, sc);

	case SIGSTOP: case SIGTSTP: case SIGTTIN: case SIGTTOU:
            /* Wait for SIGCONT to reset flag and wake us up */
            /* Really we are supposed to stop all threads in the process
             * but that's tricky so we just stop this one and hope for the best
             */
            /* Must notify parent of stop per signal specs */
            r = segment_map(start_env->process_status_seg,
                            0, SEGMAP_READ | SEGMAP_WRITE,
                            (void **) &ps, 0, 0);
            if (r < 0)
                return;
            ps->status = PROCESS_STOPPED;
            ps->stops++;
            ps->exit_signal = si->si_signo;
            kill(start_env->ppid, SIGCHLD);
            /* Stall the thread until woken up by CONT */
            while (ps->status == PROCESS_STOPPED)
                sys_sync_wait(&ps->status, PROCESS_STOPPED, UINT64(~0));
            segment_unmap(ps);
	    return;

        case SIGCONT:
            r = segment_map(start_env->process_status_seg,
                            0, SEGMAP_READ | SEGMAP_WRITE,
                            (void **) &ps, 0, 0);
            if (r < 0)
                return;
            /* Wake up stopped process */
            ps->status = PROCESS_RUNNING;
            sys_sync_wakeup(&ps->status);
            segment_unmap(ps);
            return;

	case SIGURG:  case SIGCHLD: case SIGWINCH:
	case SIGINFO:
	    return;

	default:
	    cprintf("%s: unhandled default signal %d\n",
		    jos_progname, si->si_signo);
	    exit(-1);
	}
    }

    if (signal_debug)
	cprintf("[%"PRIu64"] signal_execute: running sigaction\n", thread_id());
    sa.sa_sigaction(si->si_signo, si, sc);
}

// Execute signal handler for si.
// Called with utrap unmasked and no locks held.
// Signal is masked.
// Jumps back to sc to return.
static void __attribute__((noreturn, JOS_ONSTACK_GCCATTR))
signal_utrap_onstack(siginfo_t *si, struct sigcontext *sc)
{
    int signo = si->si_signo;
    if (signal_debug)
	cprintf("[%"PRIu64"] signal_utrap_onstack: signal %d\n",
		thread_id(), signo);

    if (signo == SIGSEGV && tls_data && tls_data->tls_pgfault_all) {
	if (signal_debug)
	    cprintf("[%"PRIu64"] signal_utrap_onstack: "
		    "longjmp to tls_pgfault_all\n",
		    thread_id());

	utrap_set_mask(1);
	jthread_mutex_lock(&sigmask_mu);
	sigdelset(&signal_masked, signo);
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);

	jos_longjmp(tls_data->tls_pgfault_all, 1);
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
    if (signal_debug) {
	cprintf("[%"PRIu64"] signal_utrap_onstack: returning\n",
		thread_id());
	if (signal_debug_utf_dump)
	    utf_dump(&sc->sc_utf);
    }

    utrap_ret(&sc->sc_utf);
}

static void __attribute__((noreturn, noinline))
signal_utrap_si(siginfo_t *si, struct sigcontext *sc)
{
    if (signal_debug)
	cprintf("[%"PRIu64"] signal_utrap_si: signal %d\n",
		thread_id(), si->si_signo);

    // Make sure sigsuspend() wakes up
    signal_counter++;
    sys_sync_wakeup(&signal_counter);

    // Push a copy of sc, si onto the trapped stack, plus the red zone.
    struct {
	struct sigcontext sc;
	siginfo_t si;
#if defined(JOS_ARCH_amd64)
	uint8_t redzone[128];
#endif
    } *s;

    struct UTrapframe *utrap_chain;
    void *stackptr;
    siginfo_t *si_arg;
    struct sigcontext *sc_arg;

    // Run signal_utrap_onstack(), which will figure out the right
    // signal handler and execute it.
    if (sc->sc_utf.utf_stackptr <= (uintptr_t) tls_stack_top &&
	sc->sc_utf.utf_stackptr > (uintptr_t) tls_base)
    {
	if (signal_debug)
	    cprintf("[%"PRIu64"] signal_utrap_si: staying on tls stack\n",
		    thread_id());

	stackptr = &s;		/* just reuse some stack pointer nearby */
	si_arg = si;
	sc_arg = sc;
	utrap_chain = &sc->sc_utf;
    } else {
	if (!sc->sc_utf.utf_stackptr) {
	    cprintf("signal_utrap_si: null stackptr!\n");
	    utf_dump(&sc->sc_utf);
	}

	// XXX assumes stack grows down
	s = (void *) sc->sc_utf.utf_stackptr;
	s--;

	// Ensure there's space, because we're masking traps right now..
	if (signal_debug)
	    cprintf("[%"PRIu64"] signal_utrap_si: pre-allocating stack at %p\n",
		    thread_id(), s);

	int r = signal_grow_stack ? stack_grow(s) : 0;
	if (r < 0) {
	    cprintf("[%"PRIu64"] signal_utrap_si: stack overflow @ %p\n",
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
	    cprintf("[%"PRIu64"] signal_utrap_si: jumping to real stack at %p\n",
		    thread_id(), s);

	stackptr = s;
	si_arg = &s->si;
	sc_arg = &s->sc;
	utrap_chain = &s->sc.sc_utf;
    }

    // Jump over there and unmask utrap atomically
    struct UTrapframe utf_jump;
#if defined(JOS_ARCH_amd64)
    utf_jump.utf_rflags = read_rflags();
    utf_jump.utf_rip = (uint64_t) &utrap_chain_dwarf2;
    utf_jump.utf_rsp = (uint64_t) stackptr;
    utf_jump.utf_rdi = (uint64_t) si_arg;
    utf_jump.utf_rsi = (uint64_t) sc_arg;
    utf_jump.utf_r14 = (uint64_t) utrap_chain;
    utf_jump.utf_r15 = (uint64_t) &signal_utrap_onstack;
#elif defined(JOS_ARCH_i386) || defined(JOS_ARCH_nacl)
    utf_jump.utf_eflags = read_eflags();
    utf_jump.utf_eip = (uintptr_t) &utrap_chain_dwarf2;
    utf_jump.utf_esp = (uintptr_t) stackptr;
    utf_jump.utf_eax = (uintptr_t) si_arg;
    utf_jump.utf_edx = (uintptr_t) sc_arg;
    utf_jump.utf_esi = (uintptr_t) utrap_chain;
    utf_jump.utf_ecx = (uintptr_t) &signal_utrap_onstack;
#elif defined(JOS_ARCH_sparc)
    utf_jump.utf_pc = (uintptr_t) &signal_utrap_onstack;
    utf_jump.utf_npc = utf_jump.utf_pc + 4;
    utf_jump.utf_regs.sp = ((uintptr_t) stackptr) - STACKFRAME_SZ;
    utf_jump.utf_regs.o0 = (uintptr_t) si_arg;
    utf_jump.utf_regs.o1 = (uintptr_t) sc_arg;
#else
#error Unknown arch
#endif

    utrap_ret(&utf_jump);
}

void
signal_utrap(struct UTrapframe *utf)
{
    if (signal_debug) {
	cprintf("[%"PRIu64"] signal_utrap: sp=0x%zx pc=0x%zx\n",
		thread_id(), utf->utf_stackptr, utf->utf_pc);
	if (signal_debug_utf_dump)
	    utf_dump(utf);
    }

    siginfo_t si;
    memset(&si, 0, sizeof(si));
    int sigmu_taken = 0;

    if (utf->utf_trap_src == UTRAP_SRC_HW) {
	si.si_addr = (void *) (uintptr_t) utf->utf_trap_arg;
	if (utf->utf_trap_num == ARCH_PGFLTD ||
	    utf->utf_trap_num == ARCH_PGFLTI) {
	    int r = stack_grow(si.si_addr);
	    if (r > 0) {
		if (signal_debug)
		    cprintf("[%"PRIu64"] signal_utrap: grew stack\n",
			    thread_id());
		return;
	    }

	    if (r == 0) {
		if (signal_debug)
		    cprintf("[%"PRIu64"] signal_utrap: stack_grow returns 0\n",
			    thread_id());
	    }

	    si.si_signo = SIGSEGV;
	    si.si_code = SEGV_ACCERR;	// maybe use segment_lookup()
	} else if (utf->utf_trap_num == ARCH_DEVICE) {
	    int r = sys_self_fp_enable();
	    if (r >= 0) {
		if (signal_debug)
		    cprintf("[%"PRIu64"] signal_utrap: enabled FP\n",
			    thread_id());
		return;
	    }

	    fprintf(stderr, "[%"PRIu64"] signal_utrap: cannot enable fp: %s\n",
		    thread_id(), e2s(r));
	    si.si_signo = SIGFPE;
	    si.si_code = FPE_FLTINV;
	} else if (utf->utf_trap_num == ARCH_BRKPT ||
		   utf->utf_trap_num == ARCH_DEBUG) {
	    si.si_signo = SIGTRAP;
	    si.si_code = TRAP_BRKPT;
	} else if (utf->utf_trap_num == ARCH_DIVIDE) {
	    si.si_signo = SIGFPE;
	    si.si_code = FPE_INTDIV;
	} else {
	    cprintf("[%"PRIu64"] signal_utrap: unknown hw trap %d\n",
		    thread_id(), utf->utf_trap_num);
	    utf_dump(utf);

	    si.si_signo = SIGILL;
	    si.si_code = ILL_ILLTRP;
	}
    } else if (utf->utf_trap_src == UTRAP_SRC_USER) {
	if (utf->utf_trap_num == UTRAP_USER_NOP)
	    return;

	if (utf->utf_trap_num != UTRAP_USER_SIGNAL) {
	    fprintf(stderr, "[%"PRIu64"] signal_utrap: unknown user trap# %d\n",
		    thread_id(), utf->utf_trap_num);
	    return;
	}

	jthread_mutex_lock(&sigmask_mu);
	sigmu_taken = 1;

	uint32_t i;
	for (i = 1; i < _NSIG; i++) {
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
		cprintf("[%"PRIu64"] signal_utrap: no pending signals\n",
			thread_id());
	    return;
	}
    } else {
	cprintf("[%"PRIu64"] signal_utrap: unknown trap src %d\n",
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
	if (signal_debug)
	    cprintf("[%"PRIu64"] kill_thread_siginfo: bad signo %d\n",
		    thread_id(), si->si_signo);
	__set_errno(EINVAL);
	return -1;
    }

    if (si->si_signo == 0) {
	if (signal_debug)
	    cprintf("[%"PRIu64"] kill_thread_siginfo: signal 0\n", thread_id());
	return 0;
    }

    int oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);

    int needtrap = 0;
    if (!sigismember(&signal_queued, si->si_signo)) {
	sigaddset(&signal_queued, si->si_signo);
	memcpy(&signal_queued_si[si->si_signo], si, sizeof(*si));
	needtrap = 1;
    }

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    if (signal_debug)
	cprintf("[%"PRIu64"] kill_thread_siginfo: needtrap %d\n",
		thread_id(), needtrap);

    // Trap the signal-processing thread so it runs the signal handler
    return needtrap ? signal_trap_thread(tobj, si->si_signo) : 0;
}

void
signal_gate_incoming(siginfo_t *si)
{
    if (signal_debug)
	cprintf("[%"PRIu64"] signal_gate_incoming: signal %d\n",
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
sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
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
sigsuspend(const sigset_t *mask)
{
    int oumask = utrap_set_mask(1);
    jthread_mutex_lock(&sigmask_mu);

    uint64_t ctr = signal_counter;
    if (signal_debug)
	cprintf("[%"PRIu64"] sigsuspend: starting, ctr=%"PRIu64"\n",
		thread_id(), ctr);

    sigset_t osigmask;
    memcpy(&osigmask, &signal_masked, sizeof(signal_masked));
    memcpy(&signal_masked, mask, sizeof(signal_masked));

    jthread_mutex_unlock(&sigmask_mu);
    utrap_set_mask(oumask);

    signal_trap_if_pending();
    while (signal_counter == ctr) {
	if (signal_debug)
	    cprintf("[%"PRIu64"] sigsuspend: waiting..\n", thread_id());
	sys_sync_wait(&signal_counter, ctr, UINT64(~0));
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
kill(pid_t pid, int sig)
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
	cprintf("[%"PRIu64"] kill_siginfo: pid %"PRIu64" signal %d\n",
		thread_id(), pid, si->si_signo);

    if (pid == 0)
	pid = -getpgrp();

    if (pid < 0) {
	int64_t procslots = sys_container_get_nslots(start_env->process_pool);
	if (procslots < 0) {
	    __set_errno(EINVAL);
	    return -1;
	}

	int sendcount = 0;
        /* TODO: for loop here to scan start_env->process_pool and 
           process pool of pid given (negative of it since it's a pgrp id */
        uint64_t procpool[2] = {start_env->process_pool,
                                sys_container_get_parent(-pid)};
        for (uint64_t pi = 0; pi < 2; pi++) {
            /* If the first pool and the second are the same only check one */
            if (pi && procpool[0] == procpool[1])
                continue;
            for (int64_t slot = 0; slot < procslots; slot++) {
                int64_t procpid = sys_container_get_slot_id(procpool[pi],
                                                            slot);
                if (procpid < 0)
                    continue;

                int send = 0;
                if (pid == -1) {
                    send = 1;
                } else {
                    pid_t pgid = __getpgid(procpid);
                    if (pgid == -pid)
                        send = 1;
                }

                if (send) {
                    kill_siginfo(procpid, si);
                    sendcount++;
                }
            }
        }

        if (sendcount == 0) {
            __set_errno(ESRCH);
            return -1;
        }

	return 0;
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
	    cprintf("[%"PRIu64"] kill_siginfo: cannot find signal gate in %"PRIu64": %s\n",
		    thread_id(), ct, e2s(gate_id));
	__set_errno(ESRCH);
	return -1;
    }

    return signal_gate_send(COBJ(ct, gate_id), si);
}

int
__sigsetjmp(sigjmp_buf env, int savemask)
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

    /*
     * Tail-call optimization is critical for this to work.
     * As a result, without -O2 this will be very broken.
     */
    return jos_setjmp(&env->__jmpbuf);
}

void
siglongjmp(sigjmp_buf env, int val)
{
    if (env->__mask_was_saved) {
	assert(0 == utrap_set_mask(1));
	jthread_mutex_lock(&sigmask_mu);
	memcpy(&signal_masked, &env->__saved_mask, sizeof(signal_masked));
	jthread_mutex_unlock(&sigmask_mu);
	utrap_set_mask(0);

	signal_trap_if_pending();
    }

    jos_longjmp(&env->__jmpbuf, val ? : 1);
}

int
_setjmp(jmp_buf env)
{
    env->__mask_was_saved = 0;
    return jos_setjmp(&env->__jmpbuf);
}

void
_longjmp(jmp_buf env, int val)
{
    jos_longjmp(&env->__jmpbuf, val ? : 1);
}

int
sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
    set_enosys();
    return -1;
}

libc_hidden_def(sigprocmask)
libc_hidden_def(sigsuspend)
libc_hidden_def(sigwaitinfo)
libc_hidden_def(kill)
strong_alias(_setjmp, setjmp)
strong_alias(_longjmp, longjmp)

