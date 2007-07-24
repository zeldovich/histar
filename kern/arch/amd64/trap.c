#include <machine/mmu.h>
#include <machine/pmap.h>
#include <machine/trap.h>
#include <machine/x86.h>
#include <machine/utrap.h>
#include <dev/picirq.h>
#include <kern/thread.h>
#include <kern/syscall.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <kern/prof.h>
#include <kern/arch.h>
#include <inc/error.h>

static uint64_t trap_user_iret_tsc;
static const struct Thread *trap_thread;
static int trap_thread_syscall_writeback;

static struct {
    char trap_entry_code[16] __attribute__ ((aligned (16)));
} trap_entry_stubs[256];

void
idt_init (void)
{
    int i;
    extern char trap_ec_entry_stub[], trap_noec_entry_stub[];

#define	SET_TRAP_GATE(i, dpl)					\
	SETGATE(idt[i], SEG_IG, GD_KT,				\
		&trap_entry_stubs[i].trap_entry_code[0], dpl)
#define	SET_TRAP_CODE(i, ec_prefix)				\
	memcpy(&trap_entry_stubs[i].trap_entry_code[0],		\
	       trap_##ec_prefix##_entry_stub, 16)

    for (i = 0; i < 0x100; i++) {
	SET_TRAP_CODE(i, noec);
	SET_TRAP_GATE(i, 0);
    }

    // Allow syscalls and breakpoints from ring 3
    SET_TRAP_GATE(T_SYSCALL, 3);
    SET_TRAP_GATE(T_BRKPT, 3);
    
    // Error-code-generating traps
    SET_TRAP_CODE(T_DBLFLT, ec);
    SET_TRAP_CODE(T_TSS,    ec);
    SET_TRAP_CODE(T_SEGNP,  ec);
    SET_TRAP_CODE(T_STACK,  ec);
    SET_TRAP_CODE(T_GPFLT,  ec);
    SET_TRAP_CODE(T_PGFLT,  ec);
    SET_TRAP_CODE(T_FPERR,  ec);

    // All ready
    lidt(&idtdesc.pd_lim);
}

static void
trapframe_print (const struct Trapframe *tf)
{
    cprintf("rax %016lx  rbx %016lx  rcx %016lx\n",
	    tf->tf_rax, tf->tf_rbx, tf->tf_rcx);
    cprintf("rdx %016lx  rsi %016lx  rdi %016lx\n",
	    tf->tf_rdx, tf->tf_rsi, tf->tf_rdi);
    cprintf("r8  %016lx  r9  %016lx  r10 %016lx\n",
	    tf->tf_r8, tf->tf_r9, tf->tf_r10);
    cprintf("r11 %016lx  r12 %016lx  r13 %016lx\n",
	    tf->tf_r11, tf->tf_r12, tf->tf_r13);
    cprintf("r14 %016lx  r15 %016lx  rbp %016lx\n",
	    tf->tf_r14, tf->tf_r15, tf->tf_rbp);
    cprintf("rip %016lx  rsp %016lx  cs %04x  ss %04x\n",
	    tf->tf_rip, tf->tf_rsp, tf->tf_cs, tf->tf_ss);
    cprintf("rflags %016lx  err %08x\n",
	    tf->tf_rflags, tf->tf_err);
}

static void
page_fault(const struct Thread *t, const struct Trapframe *tf, uint32_t err)
{
    void *fault_va = (void*) rcr2();
    uint32_t reqflags = 0;

    if ((err & FEC_W))
	reqflags |= SEGMAP_WRITE;
    if ((err & FEC_I))
	reqflags |= SEGMAP_EXEC;

    if ((tf->tf_cs & 3) == 0) {
	cprintf("kernel page fault: va=%p\n", fault_va);
	trapframe_print(tf);
	panic("kernel page fault");
    } else {
	int r = thread_pagefault(t, fault_va, reqflags);
	if (r == 0 || r == -E_RESTART)
	    return;

	r = thread_utrap(t, UTRAP_SRC_HW, T_PGFLT, (uintptr_t) fault_va);
	if (r == 0 || r == -E_RESTART)
	    return;

	cprintf("user page fault: thread %ld (%s), as %ld (%s), "
		"va=%p: rip=0x%lx, rsp=0x%lx: %s\n",
		t->th_ko.ko_id, t->th_ko.ko_name,
		t->th_as ? t->th_as->as_ko.ko_id : 0,
		t->th_as ? t->th_as->as_ko.ko_name : "null",
		fault_va, tf->tf_rip, tf->tf_rsp, e2s(r));
	thread_halt(t);
    }
}

static void
trap_dispatch(int trapno, const struct Trapframe *tf)
{
    int64_t r;

    if (trapno == T_NMI) {
	uint8_t reason = inb(0x61);
	panic("NMI, reason code 0x%x\n", reason);
    }

    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + MAX_IRQS) {
	uint32_t irqno = trapno - IRQ_OFFSET;
	irq_eoi(irqno);
	irq_handler(irqno);
	return;
    }

    if (!trap_thread) {
	trapframe_print(tf);
	panic("trap %d while idle", trapno);
    }

    switch (trapno) {
    case T_SYSCALL:
	trap_thread_syscall_writeback = 1;
	r = kern_syscall(tf->tf_rdi, tf->tf_rsi, tf->tf_rdx, tf->tf_rcx,
			 tf->tf_r8,  tf->tf_r9,  tf->tf_r10, tf->tf_r11);

	if (trap_thread_syscall_writeback) {
	    trap_thread_syscall_writeback = 0;
	    /*
	     * If the thread didn't get vectored elsewhere,
	     * write the result into the thread's registers.
	     */
	    struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	    if (r == -E_RESTART)
		t->th_tf.tf_rip -= 2;
	    else
		t->th_tf.tf_rax = r;
	} else {
	    assert(r == 0);
	}
	break;

    case T_PGFLT:
	page_fault(trap_thread, tf, tf->tf_err);
	break;

    default:
	r = thread_utrap(trap_thread, UTRAP_SRC_HW, trapno, 0);
	if (r != 0 && r != -E_RESTART) {
	    cprintf("Unknown trap %d, cannot utrap: %s.  Trapframe:\n",
		    trapno, e2s(r));
	    trapframe_print(tf);
	    thread_halt(trap_thread);
	}
    }
}

void __attribute__((__noreturn__, no_instrument_function))
trap_handler(struct Trapframe *tf, uint64_t trampoline_rip)
{
    uint64_t trap0rip = (uint64_t)&trap_entry_stubs[0].trap_entry_code[0];
    uint32_t trapno = (trampoline_rip - trap0rip) / 16;

    tf->tf_ds = read_ds();
    tf->tf_es = read_es();
    tf->tf_fs = read_fs();
    tf->tf_gs = read_gs();

    cyg_profile_free_stack(read_rsp());

    if (trap_thread) {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	sched_stop(t, read_tsc() - trap_user_iret_tsc);

	t->th_tf = *tf;
	if (t->th_fp_enabled) {
	    void *p;
	    assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_excl_dirty));
	    lcr0(rcr0() & ~CR0_TS);
	    fxsave((struct Fpregs *) p);
	}
    }

    uint64_t start = read_tsc();
    if (trap_thread) {
	prof_user(0, start - trap_user_iret_tsc);
	prof_thread(trap_thread, start - trap_user_iret_tsc);
    } else {
	prof_user(1, start - trap_user_iret_tsc);
    }

    trap_dispatch(trapno, tf);
    prof_trap(trapno, read_tsc() - start);

    thread_run();
}


static void
run_cache_flush(const struct Thread *t)
{
    static uint64_t prev_tid;
    static int prev_cflush;

    if (prev_tid != t->th_ko.ko_id && (prev_cflush || t->th_cache_flush))
	wbinvd();

    prev_tid = t->th_ko.ko_id;
    prev_cflush = t->th_cache_flush;
}

static void
trap_thread_set(const struct Thread *t)
{
    if (trap_thread) {
	kobject_unpin_hdr(&trap_thread->th_ko);
	trap_thread = 0;
    }

    if (t) {
	kobject_pin_hdr(&t->th_ko);
	trap_thread = t;
    }
}

void
thread_arch_run(const struct Thread *t)
{
    trap_thread_set(t);
    trap_user_iret_tsc = read_tsc();

    /*
     * Unclear who should get charged for the overhead of the cache flush,
     * since the real overhead is incurred when user-space code runs anyway.
     */
    run_cache_flush(t);

    if (t->th_fp_enabled) {
	void *p;
	assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_shared_ro));
	lcr0(rcr0() & ~CR0_TS);
	fxrstor((const struct Fpregs *) p);
    } else {
	lcr0(rcr0() | CR0_TS);
    }

#define LOAD_SEGMENT_REG(t, rs) \
    if (t->th_tf.tf_##rs != read_##rs()) { write_##rs(t->th_tf.tf_##rs); }

    LOAD_SEGMENT_REG(t, ds);
    LOAD_SEGMENT_REG(t, es);
    LOAD_SEGMENT_REG(t, fs);
    LOAD_SEGMENT_REG(t, gs);
#undef LOAD_SEGMENT_REG

    trapframe_pop(&t->th_tf);
}

void
thread_arch_idle(void)
{
    trap_thread_set(0);
    trap_user_iret_tsc = read_tsc();
    thread_arch_idle_asm();
}

int
thread_arch_is_masked(const struct Thread *t)
{
    return t->th_tf.tf_cs == GD_UT_MASK;
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    void *stacktop;
    uint64_t rsp = t->th_tf.tf_rsp;
    if (rsp > t->th_as->as_utrap_stack_base &&
	rsp <= t->th_as->as_utrap_stack_top)
    {
	// Skip red zone (see ABI spec)
	stacktop = (void *) (uintptr_t) rsp - 128;
    } else {
	stacktop = (void *) t->th_as->as_utrap_stack_top;
    }

    struct UTrapframe t_utf;
    t_utf.utf_trap_src = src;
    t_utf.utf_trap_num = num;
    t_utf.utf_trap_arg = arg;
#define UTF_COPY(r) t_utf.utf_##r = t->th_tf.tf_##r
    UTF_COPY(rax);  UTF_COPY(rbx);  UTF_COPY(rcx);  UTF_COPY(rdx);
    UTF_COPY(rsi);  UTF_COPY(rdi);  UTF_COPY(rbp);  UTF_COPY(rsp);
    UTF_COPY(r8);   UTF_COPY(r9);   UTF_COPY(r10);  UTF_COPY(r11);
    UTF_COPY(r12);  UTF_COPY(r13);  UTF_COPY(r14);  UTF_COPY(r15);
    UTF_COPY(rip);  UTF_COPY(rflags);
#undef UTF_COPY

    struct UTrapframe *utf = stacktop - sizeof(*utf);
    int r = check_user_access(utf, sizeof(*utf), SEGMAP_WRITE);
    if (r < 0) {
	if ((uintptr_t) utf <= t->th_as->as_utrap_stack_base)
	    cprintf("thread_arch_utrap: utrap stack overflow\n");
	return r;
    }

    if (t == trap_thread && trap_thread_syscall_writeback) {
	trap_thread_syscall_writeback = 0;
	t_utf.utf_rax = 0;
    }

    memcpy(utf, &t_utf, sizeof(*utf));
    t->th_tf.tf_rsp = (uintptr_t) utf;
    t->th_tf.tf_rip = t->th_as->as_utrap_entry;
    t->th_tf.tf_rflags &= ~FL_TF;
    t->th_tf.tf_cs = GD_UT_MASK;
    return 0;
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    return -E_INVAL;
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    if (t == trap_thread)
	trap_thread_syscall_writeback = 0;

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_rflags = FL_IF;
    t->th_tf.tf_cs = GD_UT_NMASK | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_rip = (uintptr_t) te->te_entry;
    t->th_tf.tf_rsp = (uintptr_t) te->te_stack;
    t->th_tf.tf_rdi = te->te_arg[0];
    t->th_tf.tf_rsi = te->te_arg[1];
    t->th_tf.tf_rdx = te->te_arg[2];
    t->th_tf.tf_rcx = te->te_arg[3];
    t->th_tf.tf_r8  = te->te_arg[4];
    t->th_tf.tf_r9  = te->te_arg[5];

    static_assert(thread_entry_narg == 6);
}

static void __attribute__((used))
trap_field_symbols(void)
{
#define TF_DEF(field)							\
  __asm volatile (".globl\t" #field "\n\t.set\t" #field ",%0"		\
		:: "m" (*(int *) offsetof (struct Trapframe, field)))
  TF_DEF (tf_rax);
  TF_DEF (tf_rcx);
  TF_DEF (tf_rdx);
  TF_DEF (tf_rsi);
  TF_DEF (tf_rdi);
  TF_DEF (tf_r8);
  TF_DEF (tf_r9);
  TF_DEF (tf_r10);
  TF_DEF (tf_r11);
  TF_DEF (tf_rbx);
  TF_DEF (tf_rbp);
  TF_DEF (tf_r12);
  TF_DEF (tf_r13);
  TF_DEF (tf_r14);
  TF_DEF (tf_r15);
  TF_DEF (tf_err);
  TF_DEF (tf_rip);
  TF_DEF (tf_cs);
  TF_DEF (tf_ds);
  TF_DEF (tf_es);
  TF_DEF (tf_fs);
  TF_DEF (tf_rflags);
  TF_DEF (tf_rsp);
  TF_DEF (tf_ss);
  TF_DEF (tf_gs);
  TF_DEF (tf__trapentry_rip);
}
