#ifndef _X86_64_PTRACE_H
#define _X86_64_PTRACE_H

#include <asm/ptrace-abi.h>
#include <sysdep/ptrace.h>

#ifndef __ASSEMBLY__

struct pt_regs {
    struct lind_pt_regs regs;
};

#endif

#if defined(__KERNEL__) && !defined(__ASSEMBLY__) 

extern unsigned long profile_pc(struct pt_regs *regs);

struct task_struct;

#endif

#endif
