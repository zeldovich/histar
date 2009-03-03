#include <machine/asm.h>

ENTRY(stack_curptr)
	mov	r0, sp
	mov	pc, lr

/*
 * void stack_switch(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
 *                void *stacktop, void *fn) __attribute__((noreturn));
 *
 * a0		in r0, r1
 * a1		in r2, r3
 * a3		at sp + 0
 * a4		at sp + 8
 * stacktop	at sp + 16
 * fn		at sp + 20
 */
ENTRY(stack_switch)
	ldr	r4, [sp, #20]			// r4 <- function ptr 
	ldr	r5, [sp, #16]			// r5 <- new stack top

	mov	r9, #0				// ensure 8-byte sp alignment
1:
	and	r8, r5, #0x7
	cmp	r8, r9
	beq	2f
	strb	r9, [r5, #-1]!
	b	1b
2:

	ldmia	sp, {r6-r9}			// copy a3 and a4 to new stack
	stmdb	r5, {r6-r9}
	sub	r5, r5, #16			// adjust new sp

	mov	sp, r5				// switch stacks
	mov	pc, r4				// jump to function
