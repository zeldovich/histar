#ifndef JOS_MACHINE_UTRAP_H
#define JOS_MACHINE_UTRAP_H

#include <inc/types.h>
#include <machine/mmu.h>

#define JOS_UTRAP_GCCATTR

struct UTrapframe {
	uint32_t	utf_spsr;	// saved cpsr
	uint32_t	utf_r0;
	uint32_t	utf_r1;
	uint32_t	utf_r2;
	uint32_t	utf_r3;
	uint32_t	utf_r4;
	uint32_t	utf_r5;
	uint32_t	utf_r6;
	uint32_t	utf_r7;
	uint32_t	utf_r8;
	uint32_t	utf_r9;
	uint32_t	utf_r10;
	uint32_t	utf_r11;

	union {
		uint32_t	utf_r12;
		uint32_t	utf_fp;
	};

	union {
		uint32_t	utf_r13;
		uint32_t	utf_sp;
		uint32_t	utf_stackptr;
	};

	union {
		uint32_t	utf_r14;
		uint32_t	utf_lr;
	};

	union {
		uint32_t	utf_r15;
		uint32_t	utf_pc;
	};

	uint32_t utf_trap_src;
	uint32_t utf_trap_num;
	uint64_t utf_trap_arg;
};

#define UT_MASK	 1
#define UT_NMASK 0

#endif
