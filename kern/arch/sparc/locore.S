#include <machine/asm.h>

ENTRY(start)
	mov	%l0, %l1
1:	ba 1b
	nop

trap_return:
	jmpl	%r17, %r0	/* old PC */
	rett	%r18		/* old nPC */

/*
 * XXX
 * Dummy symbols that don't actually do the right thing
 */

ENTRY(jos_longjmp)
ENTRY(jos_setjmp)
ENTRY(disk_io)
ENTRY(ide_poke)

