#include <machine/asm.h>

//-----------------------------------------------------------------------
// NB: ARM procedure calling convention requires sp to be 8-byte aligned! 
//-----------------------------------------------------------------------

ENTRY(_start)
	.cfi_startproc

	// Terminate stack unwinding
	mov	r9, #0
	str	r9, [sp, #-4]!

	// Ensure 8-byte alignment
1:
	and	r8, sp, #0x7
	cmp	r8, r9
	beq	2f
	strb	r9, [sp, #-1]!
	b	1b

2:
	// Save our three arguments
	str	r0, [sp, #-4]!
	str	r1, [sp, #-4]!
	str	r2, [sp, #-4]!

	// Load main's address and push to preserve 8-byte sp alignment
	adr	r3, _main_addr
	ldr	r3, [r3]
	str	r3, [sp, #-4]!

	// Call setup_env with our args still in r0..r2
	bl	setup_env

	// crtbegin, crtend, and whatever else
	bl	_init

	// Call libmain with the right args
	ldr	r3, [sp], #4
	ldr	r2, [sp], #4
	ldr	r1, [sp], #4
	ldr	r0, [sp], #4
	bl	libmain

_main_addr:
	.word main
	.cfi_endproc
