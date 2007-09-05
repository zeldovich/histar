#include <machine/asm.h>

ENTRY(jos_setjmp)
	st	%sp, [%o0]
	st	%o7, [%o0 + 4]
	STORE_LOCALS(o0 + 8)
	STORE_INS(o0 + 40)
	STORE_GLOBALS(o0 + 72)

	retl
	 mov	%g0, %o0

ENTRY(jos_longjmp)
	LOAD_GLOBALS(o0 + 72)
	LOAD_INS(o0 + 40)
	LOAD_LOCALS(o0 + 8)
	ld	[%o0 + 4], %o7
	ld	[%o0], %sp

	retl
	 mov	%o1, %o0
