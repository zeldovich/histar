#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>

struct UTrapRegs {
    uint64_t utf_rax;
    uint64_t utf_rbx;
    uint64_t utf_rcx;
    uint64_t utf_rdx;

    uint64_t utf_rsi;
    uint64_t utf_rdi;
    uint64_t utf_rbp;
    uint64_t utf_rsp;

    uint64_t utf_r8;
    uint64_t utf_r9;
    uint64_t utf_r10;
    uint64_t utf_r11;

    uint64_t utf_r12;
    uint64_t utf_r13;
    uint64_t utf_r14;
    uint64_t utf_r15;

    uint64_t utf_rip;
    uint64_t utf_rflags;
};

struct UTrapframe {
    struct UTrapRegs;

    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#define UTRAP_SRC_HW	0
#define UTRAP_SRC_USER	1

#endif
