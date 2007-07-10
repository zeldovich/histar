#include <machine/asm.h>

ENTRY(stack_curptr)
	retl
	 or	%sp, %g0, %o0

ENTRY(stack_switch)
	ba	stack_switch
	 nop
