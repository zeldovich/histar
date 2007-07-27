#include <kern/thread.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <machine/trap.h>
#include <machine/mmu.h>
#include <machine/sparc-common.h>
#include <machine/trapcodes.h>
#include <machine/psr.h>
#include <machine/utrap.h>
#include <dev/irqmp.h>
#include <inc/error.h>

static const struct Thread *trap_thread;

static void
trapframe_print(const struct Trapframe *tf)
{
    cprintf("       globals     outs   locals      ins\n");
    for (uint32_t i = 0; i < 8; i++) {
	cprintf("   %d: %08x %08x %08x %08x\n", 
		i, tf->tf_reg0[i], tf->tf_reg0[i + 8],
		tf->tf_reg0[i + 16], tf->tf_reg0[i + 24]);
    }
    cprintf("\n");
    cprintf(" psr: %08x  y: %08x  pc: %08x  npc: %08x\n",
	    tf->tf_psr, tf->tf_y, tf->tf_pc, tf->tf_npc);
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

	cprintf("user page fault: thread %"PRIu64" (%s), as %"PRIu64" (%s), "
		"va=%p: pc=0x%x, npc=0x%x, sp=0x%x: %s\n",
		t->th_ko.ko_id, t->th_ko.ko_name,
		t->th_as ? t->th_as->as_ko.ko_id : 0,
		t->th_as ? t->th_as->as_ko.ko_name : "null",
		fault_va, tf->tf_pc, tf->tf_npc, 0, e2s(r));

	thread_halt(t);
    }
}

static void
trap_dispatch(int trapno, const struct Trapframe *tf)
{
    if (!trap_thread) {
	trapframe_print(tf);
	panic("trap %d while idle", trapno);
    }

    if (trapno >= T_IRQOFFSET && trapno < T_IRQOFFSET + MAX_IRQS) {
	uint32_t irqno = trapno - T_IRQOFFSET;
	irqmp_clear(irqno);
	irq_handler(irqno);
	return;
    }
    
    switch(trapno) {
    case T_TEXTFAULT:
    case T_DATAFAULT:
	page_fault(trap_thread, tf, trapno);
	break;
    default:
	panic("trap %d", trapno);
    }
}

void __attribute__((__noreturn__, no_instrument_function))
trap_handler(struct Trapframe *tf, uint32_t tbr)
{
    uint32_t trapno = (tbr >> TBR_TT_SHIFT) & TBR_TT_MASK;

    if (trap_thread) {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	t->th_tf = *tf;
    }
    
    trap_dispatch(trapno, tf);    
    thread_run();
}

void
thread_arch_jump(struct Thread *t, const struct thread_entry *te)
{
    memset(&t->th_tf, 0, sizeof(t->th_tf));
    t->th_tf.tf_pc = (uintptr_t) te->te_entry;
    t->th_tf.tf_npc = (uintptr_t) (te->te_entry + 4);
    t->th_tf.tf_reg1.sp = (uintptr_t) te->te_stack;
    t->th_tf.tf_reg1.i0 = te->te_arg[0];
    t->th_tf.tf_reg1.i1 = te->te_arg[1];
    t->th_tf.tf_reg1.i2 = te->te_arg[2];
    t->th_tf.tf_reg1.i3 = te->te_arg[3];
    t->th_tf.tf_reg1.i4 = te->te_arg[4];
    t->th_tf.tf_reg1.i5 = te->te_arg[5];

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
    trapframe_pop(&t->th_tf);
}

void
thread_arch_idle(void)
{
    panic("thread_arch_idle");
}

int
thread_arch_utrap(struct Thread *t, uint32_t src, uint32_t num, uint64_t arg)
{
    return -E_INVAL;
}

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    cprintf("XXX karch_jmpbuf_init\n");
}

int
thread_arch_get_entry_args(const struct Thread *t,
			   struct thread_entry_args *targ)
{
    memcpy(targ, &t->th_tfa.tfa_entry_args, sizeof(*targ));
    return 0;
}

void
karch_fp_init(struct Fpregs *fpreg)
{
    cprintf("karch_fp_init: XXX unimpl\n");
}

int
thread_arch_is_masked(const struct Thread *t)
{
    cprintf("thread_arch_is_masked: XXX unimpl\n");
    return 0;
}
