#include <machine/asm.h>
#include <machine/mmu.h>
	
ENTRY(stack_curptr)
	retl
	 or	%sp, %g0, %o0

ENTRY(stack_switch)
	// Arg layout:
	//  %o0-%o1	64-bit arg #0
	//  %o2-%o3	64-bit arg #1
	//  %o4-%o5	64-bit arg #2
	//  %sp + 0x5C  64-bit arg #3
	//  %sp + 0x64  Target stack top
	//  %sp + 0x68  Target %pc

	// stack_switch doesn't return, so can use locals
	// setup new stack, copy over arg #3
	ld	[%sp + 0x64], %l0
	sub	%l0, STACKFRAME_SZ, %l0

	ld	[%sp + 0x5C], %l1
	st	%l1, [%l0 + 0x5C]
	ld	[%sp + 0x60], %l1
	st	%l1, [%l0 + 0x60]
	
	ld	[%sp + 0x68], %l1

	jmp	%l1
	 mov	%l0, %sp
