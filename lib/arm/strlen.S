#include <machine/asm.h>

ENTRY(strlen)
	mov	r1, r0		// save start
1:
	ldrb	r2, [r0]	// load byte
	cmp	r2, #0		// nul?
	addne	r0, r0, #1	// if not nul, increment pointer
	bne	1b		// if not nul, loop 

	sub	r0, r0, r1	// len = end - start
#ifdef __thumb__
	bx	lr		// switch modes if needed and return
#else
	mov	pc, lr		// return
#endif
