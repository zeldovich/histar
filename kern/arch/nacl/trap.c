#include <machine/mmu.h>
#include <machine/x86.h>
#include <machine/trap.h>
#include <machine/nacl.h>
#include <machine/trapcodes.h>

#include <kern/arch.h>
#include <kern/sched.h>
#include <kern/kobj.h>
#include <kern/prof.h>
#include <kern/utrap.h>
#include <kern/lib.h>
#include <inc/setjmp.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <ucontext.h>

static uint64_t trap_user_iret_tsc;
static const struct Thread *trap_thread;
static int trap_thread_syscall_writeback;

static void __attribute__((unused))
trapframe_print(const struct Trapframe *tf)
{
    printf("eax %08x  ebx %08x  ecx %08x  edx %08x\n",
	    tf->tf_eax, tf->tf_ebx, tf->tf_ecx, tf->tf_edx);
    printf("esi %08x  edi %08x  ebp %08x  esp %08x\n",
	    tf->tf_esi, tf->tf_edi, tf->tf_ebp, tf->tf_esp);
    printf("eip %08x  cs %04x  ss %04x  eflags %08x  err %08x\n",
	    tf->tf_eip, tf->tf_cs, tf->tf_ss, tf->tf_eflags, tf->tf_err);
}

static void
page_fault(const struct Thread *t, void *fault_va, 
	   const struct Trapframe *tf, uint32_t err)
{
    uint32_t reqflags = 0;
    if ((err & FEC_W))
	reqflags |= SEGMAP_WRITE;
    if ((err & FEC_I))
	reqflags |= SEGMAP_EXEC;
    
    int r = thread_pagefault(t, fault_va, reqflags);
    if (r == 0 || r == -E_RESTART)
	return;
    
    r = thread_utrap(t, UTRAP_SRC_HW, T_PGFLT, (uintptr_t) fault_va);
    if (r == 0 || r == -E_RESTART)
	return;
    
    printf("user page fault: thread %"PRIu64" (%s), "
	   "as %"PRIu64" (%s), "
	    "va=%p: eip=0x%x, esp=0x%x: %s\n",
	   t->th_ko.ko_id, t->th_ko.ko_name,
	   t->th_as ? t->th_as->as_ko.ko_id : 0,
	   t->th_as ? t->th_as->as_ko.ko_name : "null",
	   fault_va, tf->tf_eip, tf->tf_esp, e2s(r));
    thread_halt(t);
}

static void
trap_dispatch(int trapno, const struct Trapframe *tf, void *addr)
{
    if (!trap_thread) {
	trapframe_print(tf);
	assert(0);
    }

    switch (trapno) {
    case T_SYSCALL:
	assert(0);
	
    case T_PGFLT:
	page_fault(trap_thread, addr, tf, tf->tf_err);
	break;
    default:
	trapframe_print(tf);
	printf("trapno %u\n", trapno);
	assert(0);
    }
}

static void __attribute__((noreturn))
sig_handler(int num, siginfo_t *info, void *x)
{
    struct Trapframe tf;
    greg_t *greg = ((ucontext_t *)x)->uc_mcontext.gregs;
    sigset_t set;
    
    sigaddset(&set, num);
    // XXX all signals should be masked and we
    // should unmask them in some in trusted stub in the 
    // untrusted region...
    assert(sigprocmask(SIG_UNBLOCK, &set, 0) == 0);

    memset(&tf, 0, sizeof(tf));
    tf.tf_ebx = greg[REG_EBX];
    tf.tf_ecx = greg[REG_ECX];
    tf.tf_edi = greg[REG_EDI];
    tf.tf_esi = greg[REG_ESI];
    tf.tf_ebp = greg[REG_EBP];
    tf.tf_ds = greg[REG_DS];
    tf.tf_es = greg[REG_ES];
    tf.tf_edx = greg[REG_EDX];
    tf.tf_eax = greg[REG_EAX];
    
    tf.tf_err = greg[REG_ERR];
    tf.tf_eip = greg[REG_EIP];
    tf.tf_cs = greg[REG_CS];
    tf.tf_fs = greg[REG_FS];
    tf.tf_esp = greg[REG_ESP];
    tf.tf_ss = greg[REG_SS];
    tf.tf_gs = greg[REG_GS];
    tf.tf_eflags = greg[REG_EFL];
    
    int trapno = greg[REG_TRAPNO];
    
    if (trap_thread) {
	struct Thread *t = &kobject_dirty(&trap_thread->th_ko)->th;
	sched_stop(t, read_tsc() - trap_user_iret_tsc);

	t->th_tf = tf;
	
	if (t->th_fp_enabled) {
	    void *p;
	    assert(0 == kobject_get_page(&t->th_ko, 0, &p, page_excl_dirty));
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

    trap_dispatch(trapno, &tf, info->si_addr);
    prof_trap(trapno, read_tsc() - start);

    thread_run();
}

void
nacl_trap_init(void)
{
    void *va;
    struct sigaction sa;
    sa.sa_sigaction = sig_handler;
    memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;

    assert(sigaction(SIGSEGV, &sa, 0) == 0);

    assert(page_alloc(&va) == 0);
    assert(nacl_mmap((void *)USPRING, va, PGSIZE, PROT_EXEC | PROT_READ) == 0);
    memcpy(va, nacl_springboard, nacl_springboard_end - nacl_springboard);

    assert(page_alloc(&va) == 0);
    assert(nacl_mmap((void *)USCRATCH, va, PGSIZE, PROT_READ | PROT_WRITE) == 0);

    assert(page_alloc(&va) == 0);
    assert(nacl_mmap((void *)USYSCALL, va, PGSIZE, PROT_EXEC | PROT_READ) == 0);
    memcpy(va, nacl_usyscall, nacl_usyscall_end - nacl_usyscall);
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
    trap_user_iret_tsc = read_tsc();
    trap_thread_set(t);

    if (t->th_tf.tf_fs != read_fs())
	write_fs(t->th_tf.tf_fs);
    if (t->th_tf.tf_gs != read_gs())
	write_gs(t->th_tf.tf_gs);

    trapframe_pop(&t->th_tf);
}

void 
thread_arch_jump(struct Thread *t, const struct thread_entry *te) 
{
    if (t == trap_thread)
	trap_thread_syscall_writeback = 0;

    memset(&t->th_tf, 0, sizeof(t->th_tf));

    t->th_tf.tf_cs = read_cs();
    t->th_tf.tf_ss = read_ss();
    t->th_tf.tf_ds = read_ds();
    t->th_tf.tf_es = read_es();
    t->th_tf.tf_fs = read_fs();
    t->th_tf.tf_gs = read_gs();

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

    assert(thread_entry_narg >= 3);
}

void
thread_arch_idle(void)
{
    trap_thread_set(0);
    trap_user_iret_tsc = read_tsc();

    printf("thread_arch_idle\n");
    exit(0);
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
