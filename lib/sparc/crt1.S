#include <machine/asm.h>
#include <machine/mmu.h>
	
ENTRY(_start)

	// Window handler needs space for stack frame
	sub	%sp, STACKFRAME_SZ, %sp
	
	mov	%i0, %o0
	mov	%i1, %o1
	mov	%i2, %o2	

	call	setup_env
	 nop

	call	_init
	 nop

	call	libmain
	 nop
