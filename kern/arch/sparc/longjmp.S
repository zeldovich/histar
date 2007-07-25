#include <machine/asm.h>

ENTRY(jos_setjmp)
	ba jos_setjmp
	 nop

ENTRY(jos_longjmp)
	ba jos_longjmp
	 nop
