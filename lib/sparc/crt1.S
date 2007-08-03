#include <machine/asm.h>
#include <machine/mmu.h>
	
ENTRY(_start)

	// Window handler needs space for stack frame
	sub	%sp, STACKFRAME_SZ, %sp

	// Args already in %os

	call	setup_env
	 nop

	call	_init
	 nop

	call	libmain
	 nop
