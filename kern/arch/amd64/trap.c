#include <machine/mmu.h>
#include <machine/pmap.h>
#include <machine/trap.h>
#include <machine/x86.h>
#include <machine/thread.h>
#include <machine/utrap.h>
#include <dev/picirq.h>
#include <kern/syscall.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <kern/prof.h>
#include <inc/error.h>

uint64_t trap_user_iret_tsc;

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
trapframe_print (struct Trapframe *tf)
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
page_fault (struct Trapframe *tf, uint32_t err)
{
    void *fault_va = (void*) rcr2();
    uint32_t reqflags = 0;

    if ((err & FEC_W))
	reqflags |= SEGMAP_WRITE;
    if ((err & FEC_I))
	reqflags |= SEGMAP_EXEC;

    if ((tf->tf_cs & 3) == 0) {
	cprintf("kernel page fault: thread %ld (%s), va=%p, rip=0x%lx, rsp=0x%lx\n",
		cur_thread ? cur_thread->th_ko.ko_id : 0,
		cur_thread ? cur_thread->th_ko.ko_name : "(null)",
		fault_va, tf->tf_rip, tf->tf_rsp);

	panic("kernel page fault");
    } else {
	int r = thread_pagefault(cur_thread, fault_va, reqflags);
	if (r == 0 || r == -E_RESTART)
	    return;

	cprintf("user page fault: thread %ld (%s), va=%p: rip=0x%lx, rsp=0x%lx: %s\n",
		cur_thread->th_ko.ko_id, cur_thread->th_ko.ko_name,
		fault_va, tf->tf_rip, tf->tf_rsp, e2s(r));
	thread_halt(cur_thread);
    }
}

static void
trap_dispatch (int trapno, struct Trapframe *tf)
{
    int64_t r;
    uint64_t s, f;
    s = read_tsc();

    prof_user(s - trap_user_iret_tsc);

    switch (trapno) {
    case T_SYSCALL:
	r = syscall((syscall_num) tf->tf_rdi, tf->tf_rsi,
		    tf->tf_rdx, tf->tf_rcx, tf->tf_r8,
		    tf->tf_r9,  tf->tf_r10, tf->tf_r11);
	if (r != -E_RESTART)
	    tf->tf_rax = r;
	break;

    case T_PGFLT:
	page_fault(tf, tf->tf_err);
	break;

    default:
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + MAX_IRQS) {
	    irq_handler(trapno - IRQ_OFFSET);
	    break;
	}

	r = thread_utrap(cur_thread, UTRAP_SRC_HW, trapno, 0);
	if (r != 0 && r != -E_RESTART) {
	    cprintf("Unknown trap %d, cannot utrap: %s.  Trapframe:\n",
		    trapno, e2s(r));
	    trapframe_print(tf);
	    thread_halt(cur_thread);
	}
    }

    f = read_tsc();
    prof_trap(trapno, f - s);
}

void __attribute__((__noreturn__, no_instrument_function))
trap_handler (struct Trapframe *tf)
{
    uint64_t trap0rip = (uint64_t)&trap_entry_stubs[0].trap_entry_code[0];
    uint32_t trapno = (tf->tf__trapentry_rip - trap0rip) / 16;

    cyg_profile_free_stack(read_rsp());

    if (cur_thread == 0) {
	trapframe_print(tf);
	panic("trap %d with no active thread", trapno);
    }

    struct Thread *t = &kobject_dirty(&cur_thread->th_ko)->th;
    sched_stop(t);

    t->th_tf = *tf;
    if (t->th_fp_enabled) {
	void *p;
	assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_excl_dirty));
	lcr0(rcr0() & ~CR0_TS);
	fxsave((struct Fpregs *) p);
    }

    trap_dispatch(trapno, &t->th_tf);

    if (cur_thread == 0 || !SAFE_EQUAL(cur_thread->th_status, thread_runnable))
	schedule();
    thread_run(cur_thread);
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
  TF_DEF (tf_rflags);
  TF_DEF (tf_rsp);
  TF_DEF (tf_ss);
  TF_DEF (tf__trapentry_rax);
  TF_DEF (tf__trapentry_rip);
}
