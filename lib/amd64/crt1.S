#include <machine/asm.h>

ENTRY(_start)
	.cfi_startproc

	// Terminate stack unwinding
	pushq	$0

	// AMD64 requires 16-byte stack alignment before the call
	pushq	$0

	// Save our two arguments
	movq	%rdi, %r12
	movq	%rsi, %r13

	// Call setup_env with our args
	test	%rsi, %rsi
	jnz	1f
	call	setup_env

1:
	// crtbegin, crtend, and whatever else
	call	_init

	// Call libmain with the right args
	movq	%r12, %rdi
	movq	%r13, %rsi
	call	libmain
	.cfi_endproc

