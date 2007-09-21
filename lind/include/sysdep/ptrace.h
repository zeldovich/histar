#ifndef __LIND_OS_PTRACE_H
#define __LIND_OS_PTRACE_H

struct lind_pt_regs {

};

#define EMPTY_REGS { .regs = EMPTY_LIND_PT_REGS }
#define EMPTY_LIND_PT_REGS { }

#define user_mode(regs) (0)
#define user_mode_vm(regs) user_mode(regs)

#endif
