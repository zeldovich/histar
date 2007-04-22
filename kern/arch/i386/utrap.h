#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>

/*
 * The layout of this structure has to match the DWARF2 hints
 * in lib/i386/trapstub.S
 */

struct UTrapframe {
    uint32_t utf_eax;
    uint32_t utf_ebx;
    uint32_t utf_ecx;
    uint32_t utf_edx;

    uint32_t utf_esi;
    uint32_t utf_edi;
    uint32_t utf_ebp;
    union {
	uint32_t utf_esp;
	uint32_t utf_stackptr;
    };

    union {
	uint32_t utf_eip;
	uint32_t utf_pc;
    };
    uint32_t utf_eflags;

    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#define UTRAP_SRC_HW	1
#define UTRAP_SRC_USER	2

#endif
