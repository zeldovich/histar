#include <machine/asm.h>

ENTRY(_start)
	.cfi_startproc

	// Terminate stack unwinding
	pushl	$0

	// Save our three arguments, pass them on the stack
	pushl	%ecx
	pushl	%edx
	pushl	%eax

	// Call setup_env with our args
	call	setup_env

	// crtbegin, crtend, and whatever else
	call	_init

	// Call libmain with the right args
	call	libmain
	.cfi_endproc

