#ifndef JOS_MACHINE_SETJMP_H
#define JOS_MACHINE_SETJMP_H

#include <inc/types.h>

#define JOS_LONGJMP_GCCATTR

// r0-r3 are args/return/scratch, r15 is pc
struct jos_jmp_buf {
	uint32_t regs[11];	// r4 through r14
};

#endif
