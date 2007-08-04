#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>
#include <machine/mmu.h>

#define JOS_UTRAP_GCCATTR

struct UTrapframe {
    // XXX
#define utf_stackptr utf_reg1.sp
    union {
	uint32_t utf_reg0[32];
	struct Regs utf_reg1;
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
