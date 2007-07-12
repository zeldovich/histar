/*
 * include/asm-x86_64/processor.h
 *
 * Copyright (C) 1994 Linus Torvalds
 */

#ifndef __ASM_LIND_PROCESSOR_H
#define __ASM_LIND_PROCESSOR_H

#include <asm/segment.h>
#include <asm/page.h>
#include <asm/types.h>
#include <asm/sigcontext.h>
#include <asm/cpufeature.h>
#include <linux/threads.h>
#include <asm/msr.h>
#include <asm/current.h>
#include <asm/system.h>
#include <asm/mmsegment.h>
#include <asm/percpu.h>
#include <linux/personality.h>
#include <linux/cpumask.h>

#include <asm/ptrace.h>

#include <sysdep/setjmp.h>
#include <archenv.h>

struct cpuinfo_lind {
    unsigned long loops_per_jiffy;
    int ipi_pipe[2];
};

#define current_text_addr() \
	({ void *pc; __asm__("movq $1f,%0\n1:":"=g" (pc)); pc; })

struct arch_thread {
        unsigned long debugregs[8];
        int debugregs_seq;
	unsigned long fs;
};

/* REP NOP (PAUSE) is a good thing to insert into busy-wait loops. */
static inline void rep_nop(void)
{
	__asm__ __volatile__("rep;nop": : :"memory");
}

#define cpu_relax()   rep_nop()

#define INIT_ARCH_THREAD { \
	.debugregs  		= { [ 0 ... 7 ] = 0 }, \
	.debugregs_seq		= 0, \
}

struct thread_struct {
	/* This flag is set to 1 before calling do_fork (and analyzed in
	 * copy_thread) to mark that we are begin called from userspace (fork /
	 * vfork / clone), and reset to 0 after. It is left to 0 when called
	 * from kernelspace (i.e. kernel_thread() or fork_idle(), as of 2.6.11). */
	struct task_struct *saved_task;
	int forking;
	int nsyscalls;
	struct pt_regs regs;
	int singlestep_syscall;
	void *fault_addr;
	void *fault_catcher;
	struct task_struct *prev_sched;
	unsigned long temp_stack;
	void *exec_buf;
	struct arch_thread arch;

        jmp_buf switch_buf;

	struct {
		int op;
		union {
			struct {
				int pid;
			} fork, exec;
			struct {
				int (*proc)(void *);
				void *arg;
			} thread;
			struct {
				void (*proc)(void *);
				void *arg;
			} cb;
		} u;
	} request;
};

#define INIT_THREAD \
{ \
	.forking		= 0, \
	.nsyscalls		= 0, \
        .regs		   	= EMPTY_REGS, \
	.fault_addr		= NULL, \
	.prev_sched		= NULL, \
	.temp_stack		= 0, \
	.exec_buf		= NULL, \
	.arch			= INIT_ARCH_THREAD, \
	.request		= { 0 } \
}

extern struct task_struct *alloc_task_struct(void);

extern long kernel_thread(int (*fn)(void *), void * arg, unsigned long flags);

static inline void prepare_to_copy(struct task_struct *tsk)
{
}


extern unsigned long thread_saved_pc(struct task_struct *t);

static inline void mm_copy_segments(struct mm_struct *from_mm,
				    struct mm_struct *new_mm)
{
}

#define init_stack	(init_thread_union.stack)

/*
 * User space process size: 3.75GB. 
 */
#define TASK_SIZE	(0xF0000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's. We won't be using it
 */
#define TASK_UNMAPPED_BASE	0

extern void start_thread(struct pt_regs *regs, unsigned long entry, 
			 unsigned long stack);

static inline void exit_thread(void)
{
    /* nothing to do */
}

static inline void release_thread(struct task_struct *dead_task)
{
    /* nothing to do */
}

struct cpuinfo_um {
	unsigned long loops_per_jiffy;
	int ipi_pipe[2];
};

extern struct cpuinfo_lind boot_cpu_data;

#define my_cpu_data		cpu_data[smp_processor_id()]

#ifdef CONFIG_SMP
extern struct cpuinfo_um cpu_data[];
#define current_cpu_data cpu_data[smp_processor_id()]
#else
#define cpu_data (&boot_cpu_data)
#define current_cpu_data boot_cpu_data
#endif

#define KSTK_REG(tsk, reg) (0xbadbabe)
#define KSTK_EIP(tsk) KSTK_REG(tsk, EIP)
#define KSTK_ESP(tsk) KSTK_REG(tsk, UESP)

#define get_wchan(p) (0)

#endif /* __ASM_X86_64_PROCESSOR_H */
