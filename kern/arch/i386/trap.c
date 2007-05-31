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
idt_init(void)
{
    int i;
    extern char trap_ec_entry_stub[], trap_noec_entry_stub[];

#define	SET_TRAP_GATE(i, dpl)						\
	idt[i].gd = GATE32(SEG_IG, GD_KT,				\
		(uintptr_t) &trap_entry_stubs[i].trap_entry_code[0], dpl)
#define	SET_TRAP_CODE(i, ec_prefix)					\
	memcpy(&trap_entry_stubs[i].trap_entry_code[0],			\
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
trapframe_print(const struct Trapframe *tf)
{
    cprintf("eax %08x  ebx %08x  ecx %08x  edx %08x\n",
	    tf->tf_eax, tf->tf_ebx, tf->tf_ecx, tf->tf_edx);
    cprintf("esi %08x  edi %08x  ebp %08x  esp %08x\n",
	    tf->tf_esi, tf->tf_edi, tf->tf_ebp, tf->tf_esp);
    cprintf("eip %08x  cs %04x  ss %04x  eflags %08x  err %08x\n",
	    tf->tf_eip, tf->tf_cs, tf->tf_ss, tf->tf_eflags, tf->tf_err);
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

	cprintf("user page fault: thread %"PRIu64" (%s), "
		"va=%p: eip=0x%x, rsp=0x%x: %s\n",
		t->th_ko.ko_id, t->th_ko.ko_name,
		fault_va, tf->tf_eip, tf->tf_esp, e2s(r));
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
	irq_handler(trapno - IRQ_OFFSET);
	return;
    }

    if (!trap_thread) {
	trapframe_print(tf);
	panic("trap %d while idle", trapno);
    }

    switch (trapno) {
    case T_SYSCALL: {
	trap_thread_syscall_writeback = 1;

	uint32_t sysnum = tf->tf_eax;
	uint64_t *args = (uint64_t *) tf->tf_edx;
	r = check_user_access(args, sizeof(uint64_t) * 7, 0);
	if (r >= 0)
	    r = kern_syscall(sysnum, args[0], args[1], args[2],
			     args[3], args[4], args[5], args[6]);

	if (trap_thread_syscall_writeback) {
	    trap_thread_syscall_writeback = 0;
	    struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	    if (r == -E_RESTART) {
		t->th_tf.tf_eip -= 2;
	    } else {
		t->th_tf.tf_eax = ((uint64_t) r) & 0xffffffff;
		t->th_tf.tf_edx = ((uint64_t) r) >> 32;
	    }
	} else {
	    assert(r == 0);
	}
	break;
    }

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

void __attribute__((noreturn, no_instrument_function, regparm(2)))
trap_handler(struct Trapframe *tf, uint32_t trampoline_eip)
{
    uint32_t trap0eip = (uint32_t) &trap_entry_stubs[0].trap_entry_code[0];
    uint32_t trapno = (trampoline_eip - trap0eip) / 16;

    tf->tf_fs = read_fs();
    tf->tf_gs = read_gs();

    cyg_profile_free_stack(read_esp());

    if (trap_thread) {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	sched_stop(t, read_tsc());

	t->th_tf = *tf;
	if (t->th_fp_enabled) {
	    void *p;
	    assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_excl_dirty));
	    lcr0(rcr0() & ~CR0_TS);
	    fxsave((struct Fpregs *) p);
	}
    }

    uint64_t start = read_tsc();
    prof_user(start - trap_user_iret_tsc);
    prof_thread(trap_thread, start - trap_user_iret_tsc);

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

    if (t->th_tf.tf_fs != read_fs())
	write_fs(t->th_tf.tf_fs);
    if (t->th_tf.tf_gs != read_gs())
	write_gs(t->th_tf.tf_gs);

    sched_start(t, read_tsc());
    trapframe_pop(&t->th_tf);
}

void
thread_arch_idle(void)
{
    trap_thread_set(0);
    thread_arch_idle_asm();
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    void *stacktop;
    uint32_t esp = t->th_tf.tf_esp;
    if (esp > t->th_as->as_utrap_stack_base &&
	esp <= t->th_as->as_utrap_stack_top)
    {
	stacktop = (void *) (uintptr_t) esp;
    } else {
	stacktop = (void *) t->th_as->as_utrap_stack_top;
    }

    struct UTrapframe t_utf;
    t_utf.utf_trap_src = src;
    t_utf.utf_trap_num = num;
    t_utf.utf_trap_arg = arg;
#define UTF_COPY(r) t_utf.utf_##r = t->th_tf.tf_##r
    UTF_COPY(eax);  UTF_COPY(ebx);  UTF_COPY(ecx);  UTF_COPY(edx);
    UTF_COPY(esi);  UTF_COPY(edi);  UTF_COPY(ebp);  UTF_COPY(esp);
    UTF_COPY(eip);  UTF_COPY(eflags);
#undef UTF_COPY

    struct UTrapframe *utf = stacktop - sizeof(*utf);
    int r = check_user_access(utf, sizeof(*utf), SEGMAP_WRITE);
    if (r < 0) {
	if ((uintptr_t) utf <= t->th_as->as_utrap_stack_base)
	    cprintf("thread_arch_utrap: utrap stack overflow\n");
	return r;
    }

    if (t == trap_thread) {
	trap_thread_syscall_writeback = 0;
	t_utf.utf_eax = 0;
	t_utf.utf_edx = 0;
    }

    memcpy(utf, &t_utf, sizeof(*utf));
    t->th_tf.tf_esp = (uintptr_t) utf;
    t->th_tf.tf_eip = t->th_as->as_utrap_entry;
    t->th_tf.tf_eflags &= ~FL_TF;
    t->th_tf.tf_cs = GD_UT_MASK;
    return 0;
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    if (t == trap_thread)
	trap_thread_syscall_writeback = 0;

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_eflags = FL_IF;
    t->th_tf.tf_cs = GD_UT_NMASK | 3;
    t->th_tf.tf_ss = GD_UD | 3;
    t->th_tf.tf_ds = GD_UD | 3;
    t->th_tf.tf_es = GD_UD | 3;
    t->th_tf.tf_fs = GD_TD | 3;
    t->th_tf.tf_gs = GD_TD | 3;
    t->th_tf.tf_eip = (uintptr_t) te->te_entry;
    t->th_tf.tf_esp = (uintptr_t) te->te_stack;

    for (uint32_t i = 0; i < thread_entry_narg; i++)
	t->th_tfa.tfa_entry_args.te_arg[i] = te->te_arg[i];

    /*
     * As an optimization, pass first 3 arguments truncated to 32 bits.
     * gcc allows taking 3 register args using __attribute__((regparm(3))).
     */
    t->th_tf.tf_eax = te->te_arg[0];
    t->th_tf.tf_edx = te->te_arg[1];
    t->th_tf.tf_ecx = te->te_arg[2];

    static_assert(thread_entry_narg >= 3);
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}

static void __attribute__((used))
trap_field_symbols(void)
{
#define TF_DEF(field)							\
  __asm volatile (".globl\t" #field "\n\t.set\t" #field ",%0"		\
		:: "m" (*(int *) offsetof (struct Trapframe, field)))
  TF_DEF (tf_eax);
  TF_DEF (tf_ebx);
  TF_DEF (tf_ecx);
  TF_DEF (tf_edx);
  TF_DEF (tf_esi);
  TF_DEF (tf_edi);
  TF_DEF (tf_ebp);
  TF_DEF (tf_err);
  TF_DEF (tf_eip);
  TF_DEF (tf_cs);
  TF_DEF (tf_ds);
  TF_DEF (tf_es);
  TF_DEF (tf_fs);
  TF_DEF (tf_eflags);
  TF_DEF (tf_esp);
  TF_DEF (tf_ss);
  TF_DEF (tf_gs);
  TF_DEF (tf__trapentry_eip);
}
