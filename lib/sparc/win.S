#include <machine/asm.h>

/* Save handler 
 * %g5, %g6 hold pc, npc
 */
window_overflow:
	STORE_WINDOW(sp)
	/* Can use locals now */

	/* Read inst op */
	ld	[%g5], %l7

	/* We only handle save %sp, -imm13, %sp. */
	set	0x9DE3B, %l5 /* (save %sp, -imm13, %sp) >> 12 */
	srl	%l7, 12, %l6
	cmp	%l6, %l5
	bne	overflow_bail
	 /* put imm13 in %l6, removing sign bit*/
	 orn	%g0, %l7, %l6
	sll	%l6, 20, %l6
	srl	%l6, 20, %l6
	
	mov	%o0, %i0
	mov	%o1, %i1
	mov	%o2, %i2	
	mov	%o3, %i3
	mov	%o4, %i4
	mov	%o5, %i5
	mov	%o6, %i6
	mov	%o7, %i7

	sub	%sp, %l6, %sp
	dec	%sp /* remember two's complement */
	
	jmp	%g5
	 jmp	%g6
	  nop

overflow_bail:
	/* XXX */
	ba	overflow_bail
	 nop
