#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <kern/syscall.h>
#include <kern/utrap.h>
#include <machine/trap.h>
#include <machine/mmu.h>
#include <machine/sparc-common.h>
#include <machine/trapcodes.h>
#include <machine/psr.h>
#include <dev/irqmp.h>
#include <inc/error.h>

static uint64_t trap_user_iret_tsc;
static const struct Thread *trap_thread;
static int trap_thread_syscall_writeback;
static int in_idle;

static void
print_state(const char *s, const struct Thread *t)
{
    cprintf("%s: thread %"PRIu64" (%s), as %"PRIu64" (%s), "
	    "pc=0x%x, npc=0x%x, fp=0x%x, sp=0x%x",
	    s, t->th_ko.ko_id, t->th_ko.ko_name,
	    t->th_as ? t->th_as->as_ko.ko_id : 0,
	    t->th_as ? t->th_as->as_ko.ko_name : "null",
	    t->th_tf.tf_pc, t->th_tf.tf_npc, t->th_tf.tf_regs.fp, 
	    t->th_tf.tf_regs.sp);
}

static void
trapframe_print(const struct Trapframe *tf)
{
    cprintf("       globals     outs   locals      ins\n");
    for (uint32_t i = 0; i < 8; i++) {
	cprintf("   %d: %08x %08x %08x %08x\n", 
		i, tf->tf_reg[i], tf->tf_reg[i + 8],
		tf->tf_reg[i + 16], tf->tf_reg[i + 24]);
    }
    cprintf("\n");
    cprintf(" psr: %08x  y: %08x  pc: %08x  npc: %08x  wim: %08x\n",
	    tf->tf_psr, tf->tf_y, tf->tf_pc, tf->tf_npc, tf->tf_wim);
}

static void
fp_backtrace(uint32_t fp)
{
    cprintf("Backtrace:\n");
    uint32_t *fpp;

 again:
    fpp = (uint32_t *) fp;
    int r = check_user_access(fpp, 4 * 16, 0);
    if (r < 0)
	return;

    uint32_t rfp = fpp[8 + 6];
    uint32_t rpc = fpp[8 + 7];
    cprintf("  fp=%x pc=%x\n", rfp, rpc);
    if (!rfp || !rpc)
	return;

    fp = rfp;
    goto again;
}

static void
page_fault(const struct Thread *t, const struct Trapframe *tf, uint32_t trapno)
{
    void *fault_va = (void *)lda_mmuregs(SRMMU_FAULT_ADDR);
    uint32_t access_type = 
	(lda_mmuregs(SRMMU_FAULT_STATUS) >> SRMMU_AT_SHIFT) & SRMMU_AT_MASK;
    uint32_t reqflags = 0;

    if ((access_type == SRMMU_AT_SUD) || (access_type == SRMMU_AT_SUI))
	reqflags |= SEGMAP_WRITE;
    else if (access_type == SRMMU_AT_LUI)
	reqflags |= SEGMAP_EXEC;
    
    if (tf->tf_psr & PSR_PS) {
	cprintf("kernel page fault: va=%p\n", fault_va);
	trapframe_print(tf);
	panic("kernel page fault");
    } else {
	int r = thread_pagefault(t, fault_va, reqflags);
	if (r == 0 || r == -E_RESTART)
	    return;

	r = thread_utrap(t, UTRAP_SRC_HW, trapno, (uintptr_t) fault_va);
	if (r == 0 || r == -E_RESTART)
	    return;

	print_state("user page fault", t);
	cprintf(", va=%p: %s\n", fault_va, e2s(r));
	trapframe_print(&t->th_tf);
	fp_backtrace(t->th_tf.tf_regs.i6);
	thread_halt(t);
    }
}

static void
emu_error(const struct Thread *t)
{
    print_state("emulate error", t);
    cprintf(", inst=0x%08x, b", t->th_tf.tf_regs.o0);
    for (int i = 31; i >= 0; i--) {
	uint32_t m = 1 << i;
	const char *s = m & t->th_tf.tf_regs.o0 ? "1" : "0";
	cprintf("%s", s);
    }
    cprintf(", inst pc=0x%x\n", t->th_tf.tf_regs.o1);
    thread_halt(t);
}

static void
trap_dispatch(int trapno, const struct Trapframe *tf)
{
    int64_t r;

    if (trapno >= T_IRQOFFSET && trapno < T_IRQOFFSET + MAX_IRQS) {
	uint32_t irqno = trapno - T_IRQOFFSET;
	irqmp_clear(irqno);
	irq_handler(irqno);
	return;
    }

    if (!trap_thread) {
	trapframe_print(tf);
	panic("trap 0x%x while idle", trapno);
    }
    
    switch(trapno) {
    case T_SYSCALL: {
	trap_thread_syscall_writeback = 1;

	uint32_t sysnum = tf->tf_regs.i0;
#define MAKE_UINT64(a, b) (((uint64_t)(a) << 32) | (uint64_t)(b))	    
	uint64_t arg0 = MAKE_UINT64(tf->tf_regs.i1, tf->tf_regs.i2);
	uint64_t arg1 = MAKE_UINT64(tf->tf_regs.i3, tf->tf_regs.i4);
	uint64_t arg2 = MAKE_UINT64(tf->tf_regs.i5, tf->tf_regs.l0);
	uint64_t arg3 = MAKE_UINT64(tf->tf_regs.l1, tf->tf_regs.l2);
	uint64_t arg4 = MAKE_UINT64(tf->tf_regs.l3, tf->tf_regs.l4);
	uint64_t arg5 = MAKE_UINT64(tf->tf_regs.l5, tf->tf_regs.l6);
	uint64_t arg6 = MAKE_UINT64(tf->tf_regs.l7, tf->tf_regs.o0);
#undef MAKE_UINT64
	r = kern_syscall(sysnum, arg0, arg1, arg2,
			 arg3, arg4, arg5, arg6);
	
	if (trap_thread_syscall_writeback) {
	    trap_thread_syscall_writeback = 0;
	    
	    if (r != -E_RESTART) {
		struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
		t->th_tf.tf_regs.o1 = ((uint64_t) r) >> 32;
		t->th_tf.tf_regs.o2 = ((uint64_t) r) & 0xffffffff;
		t->th_tf.tf_pc = t->th_tf.tf_npc;
		t->th_tf.tf_npc = t->th_tf.tf_npc + 4;
	    } 
	} else {
	    assert(r == 0);
	}
	break;
    }
    case T_TEXTFAULT:
    case T_DATAFAULT:
	page_fault(trap_thread, tf, trapno);
	break;
    case T_EMUERR:
	emu_error(trap_thread);
	break;
    case T_FLUSHWIN: {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	t->th_tf.tf_pc = t->th_tf.tf_npc;
	t->th_tf.tf_npc = t->th_tf.tf_npc + 4;
	break;
    }
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
trap_handler(struct Trapframe *tf, uint32_t tbr)
{
    uint32_t trapno = (tbr >> TBR_TT_SHIFT) & TBR_TT_MASK;

    if (!in_idle && (tf->tf_psr & PSR_PS)) {
	cprintf("trap in supervisor mode\n");
	cprintf("trapno = %d\n", trapno);
	trapframe_print(tf);
	panic("cannot continue");
    }

    in_idle = 0;

    if (trap_thread) {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	sched_stop(t, karch_get_tsc() - trap_user_iret_tsc);
	t->th_tf = *tf;
    }

    trap_dispatch(trapno, tf);    
    thread_run();
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    if (t == trap_thread)
	trap_thread_syscall_writeback = 0;

    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_pc = (uintptr_t) te->te_entry;
    t->th_tf.tf_npc = (uintptr_t) (te->te_entry + 4);
    t->th_tf.tf_regs.g7 = UT_NMASK;
    t->th_tf.tf_wim = 0xFE;
    t->th_tf.tf_psr = PSR_S | ((NWINDOWS - 1) << PSR_CWP_SHIFT);
    t->th_tf.tf_regs.sp = (uintptr_t) te->te_stack - STACKFRAME_SZ;
    t->th_tf.tf_regs.o0 = te->te_arg[0];
    t->th_tf.tf_regs.o1 = te->te_arg[1];
    t->th_tf.tf_regs.o2 = te->te_arg[2];
    t->th_tf.tf_regs.o3 = te->te_arg[3];
    t->th_tf.tf_regs.o4 = te->te_arg[4];
    t->th_tf.tf_regs.o5 = te->te_arg[5];
    
    for (uint32_t i = 0; i < thread_entry_narg; i++)
	t->th_tfa.tfa_entry_args.te_arg[i] = te->te_arg[i];
    
    static_assert(thread_entry_narg == 6);
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
    trap_user_iret_tsc = karch_get_tsc();
    trapframe_pop(&t->th_tf);
}

void
thread_arch_idle(void)
{
    trap_thread_set(0);
    trap_user_iret_tsc = karch_get_tsc();

    in_idle = 1;
    thread_arch_idle_asm();
}

int
thread_arch_is_masked(const struct Thread *t)
{
    if (t->th_tf.tf_regs.g7 == UT_MASK)
	return 1;
    if (t->th_tf.tf_pc >= UTRAPMASKED && t->th_tf.tf_pc < (UTRAPMASKED + PGSIZE))
	return 1;
    return 0;
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    void *stacktop;
    uint32_t sp = t->th_tf.tf_regs.sp;
    if (sp > t->th_as->as_utrap_stack_base &&
	sp <= t->th_as->as_utrap_stack_top)
	stacktop = (void *) sp;
    else
	stacktop = (void *) t->th_as->as_utrap_stack_top;

    struct UTrapframe t_utf;
    t_utf.utf_trap_src = src;
    t_utf.utf_trap_num = num;
    t_utf.utf_trap_arg = arg;

    memcpy(&t_utf.utf_regs, &t->th_tf.tf_regs, sizeof(t_utf.utf_regs));
    t_utf.utf_pc = t->th_tf.tf_pc;
    t_utf.utf_npc = t->th_tf.tf_npc;
    t_utf.utf_y = t->th_tf.tf_y;

    struct UTrapframe *utf = stacktop - sizeof(*utf);
    int r = check_user_access(utf, sizeof(*utf), SEGMAP_WRITE);
    if (r < 0) {
	if ((uintptr_t) utf <= t->th_as->as_utrap_stack_base)
	    cprintf("thread_arch_utrap: utrap stack overflow\n");
	return r;
    }

    if (t == trap_thread && trap_thread_syscall_writeback) {
	trap_thread_syscall_writeback = 0;
	t_utf.utf_regs.o1 = 0;
	t_utf.utf_regs.o2 = 0;
	t_utf.utf_pc  = t_utf.utf_npc;
	t_utf.utf_npc = t_utf.utf_npc + 4;
    }

    memcpy(utf, &t_utf, sizeof(*utf));
    t->th_tf.tf_regs.sp = (uintptr_t) utf;
    t->th_tf.tf_pc = t->th_as->as_utrap_entry;
    t->th_tf.tf_npc = t->th_tf.tf_pc + 4;
    t->th_tf.tf_regs.g7 = UT_MASK;

    return 0;
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}
