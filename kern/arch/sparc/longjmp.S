#include <machine/mmu.h>
#include <machine/asm.h>

ENTRY(jos_setjmp)
	st	%sp, [%o0]
	st	%o7, [%o0 + 4]

	retl
	 mov	%g0, %o0

ENTRY(jos_longjmp)
	FLUSH_WINDOWS
	ld	[%o0], %fp
	ld	[%o0 + 4], %o7

	retl
	 restore
