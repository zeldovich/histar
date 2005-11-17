#include "kern/lib.h"
#include "machine/mmu.h"
#include <machine/pmap.h>
#include <machine/trap.h>
#include <machine/x86.h>
#include <kern/syscall.h>

static struct {
    char trap_entry_code[16] __attribute__ ((aligned (16)));
} trap_entry_stubs[256];

void
idt_init (void)
{
    int i;
    extern char trap_ec_entry_stub[], trap_noec_entry_stub[];

#define	SET_TRAP_GATE(i, dpl)		\
	SETGATE(idt[i], SEG_TG, GD_KT, &trap_entry_stubs[i].trap_entry_code[0], dpl)
#define	SET_TRAP_CODE(i, ec_prefix)	\
	memcpy(&trap_entry_stubs[i].trap_entry_code[0], trap_##ec_prefix##_entry_stub, 16)

    for (i = 0; i < 0x100; i++) {
	SET_TRAP_CODE(i, noec);
	SET_TRAP_GATE(i, 0);
    }

    // Allow syscalls from ring 3
    SET_TRAP_GATE(T_SYSCALL, 3);

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
    cprintf("rax %016lx  rbx %016lx  rcx %016lx\n", tf->tf_rax, tf->tf_rbx, tf->tf_rcx);
    cprintf("rdx %016lx  rsi %016lx  rdi %016lx\n", tf->tf_rdx, tf->tf_rsi, tf->tf_rdi);
    cprintf("r8  %016lx  r9  %016lx  r10 %016lx\n", tf->tf_r8, tf->tf_r9, tf->tf_r10);
    cprintf("r11 %016lx  r12 %016lx  r13 %016lx\n", tf->tf_r11, tf->tf_r12, tf->tf_r13);
    cprintf("r14 %016lx  r15 %016lx  rbp %016lx\n", tf->tf_r14, tf->tf_r15, tf->tf_rbp);
    cprintf("rip %016lx  rsp %016lx  cs %04x  ss %04x\n", tf->tf_rip, tf->tf_rsp, tf->tf_cs, tf->tf_ss);
    cprintf("rflags %016lx  err %08x\n", tf->tf_rflags, tf->tf_err);
}

int page_fault_mode = PFM_NONE;
static void
page_fault (struct Trapframe *tf)
{
    physaddr_t fault_va = rcr2();

    if ((tf->tf_cs & 3) == 0) {
	if (page_fault_mode == PFM_KILL) {
	    cprintf("user-triggered kernel page fault, should kill thread\n");
	    for (;;)
		;
	} else {
	    panic("kernel page fault at VA %lx", fault_va);
	}
    } else {
	cprintf("user process page-faulted at %lx\n", fault_va);
	for (;;)
	    ;
    }
}

void
trap_handler (struct Trapframe *tf)
{
    uint32_t trapno = (tf->tf__trapentry_rip - (uint64_t)&trap_entry_stubs[0].trap_entry_code[0]) / 16;

    switch (trapno) {
	case T_SYSCALL:
	    tf->tf_rax =
		syscall(tf->tf_rdi, tf->tf_rsi, tf->tf_rdx,
			tf->tf_rcx, tf->tf_r8, tf->tf_r9);
	    break;

	case T_PGFLT:
	    page_fault(tf);
	    break;

	default:
	    cprintf("Unknown trap %d, trapframe:\n", trapno);
	    trapframe_print(tf);
	    // XXX should probably kill user thread
    }
}

static void __attribute__((__unused__))
trap_field_symbols (void)
{
#define TF_DEF(field)							\
  asm volatile (".globl\t" #field "\n\t.set\t" #field ",%0"		\
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

  asm volatile (".globl\ttf_size\n\t.set\ttf_size,%0"
		:: "m" (*(int *) sizeof (struct Trapframe)));
}
