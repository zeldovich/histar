#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>
#include <machine/mmu.h>

#define JOS_UTRAP_GCCATTR
#define utf_stackptr utf_regs.sp

struct UTrapframe {
    union {
	uint32_t utf_reg[32];
	struct Regs utf_regs;
    };

    uint32_t utf_pc;
    uint32_t utf_npc;
    uint32_t utf_y;
    
    uint32_t utf_trap_src;
    uint32_t utf_trap_num;
    uint64_t utf_trap_arg;
};

#define UT_MASK 0
#define UT_NMASK 1

#endif
