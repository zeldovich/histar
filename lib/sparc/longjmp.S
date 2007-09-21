#include <machine/asm.h>

ENTRY(jos_setjmp)
	st	%sp, [%o0]
	st	%o7, [%o0 + 4]
	STORE_REG_SET(l, o0 + 8)
	STORE_REG_SET(i, o0 + 40)
	STORE_REG_SET(g, o0 + 72)

	retl
	 mov	%g0, %o0

ENTRY(jos_longjmp)
	LOAD_REG_SET(g, o0 + 72)
	LOAD_REG_SET(i, o0 + 40)
	LOAD_REG_SET(l, o0 + 8)
	ld	[%o0 + 4], %o7
	ld	[%o0], %sp

	retl
	 mov	%o1, %o0
