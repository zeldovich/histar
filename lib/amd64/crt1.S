#include <machine/asm.h>

ENTRY(_start)
	// Terminate stack unwinding
	pushq	$0

	// Save our two arguments
	movq	%rdi, %r12
	movq	%rsi, %r13

	// crtbegin, crtend, and whatever else
	call	_init

	// Call libmain with the right args
	movq	%r12, %rdi
	movq	%r13, %rsi
	call	libmain

