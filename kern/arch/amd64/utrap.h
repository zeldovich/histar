#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>

#define JOS_UTRAP_GCCATTR regparm(1)

/*
 * The layout of this structure has to match the DWARF2 hints
 * in lib/amd64/trapstub.S
 */

struct UTrapframe {
    uint64_t utf_rax;
    uint64_t utf_rbx;
    uint64_t utf_rcx;
    uint64_t utf_rdx;

    uint64_t utf_rsi;
    uint64_t utf_rdi;
    uint64_t utf_rbp;
    union {
	uint64_t utf_rsp;
	uint64_t utf_stackptr;
    };

    uint64_t utf_r8;
    uint64_t utf_r9;
    uint64_t utf_r10;
    uint64_t utf_r11;

    uint64_t utf_r12;
    uint64_t utf_r13;
    uint64_t utf_r14;
    uint64_t utf_r15;

    union {
	uint64_t utf_rip;
	uint64_t utf_pc;
    };
    uint64_t utf_rflags;

    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#define UTRAP_SRC_HW	1
#define UTRAP_SRC_USER	2

#endif
