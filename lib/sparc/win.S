#include <machine/mmu.h>
#include <machine/asm.h>
#include <machine/trapcodes.h>

#define SHIFT_O2I \
	mov	%o0, %i0; \
	mov	%o1, %i1; \
	mov	%o2, %i2; \
	mov	%o3, %i3; \
	mov	%o4, %i4; \
	mov	%o5, %i5; \
	mov	%o6, %i6; \
	mov	%o7, %i7;

/* Window handlers that emulate save and restore
 * %y, %g5 hold pc, npc
 */

.section .uwin_overflow, "ax", @progbits
.globl uwin_overflow
uwin_overflow:
	STORE_WINDOW(sp)
	/* Can use locals now */

	/* Read inst op */
	mov	%y, %l6
	ld	[%l6], %l7

	/* We only handle: 
	 *  save %sp, -imm13, %sp
	 *  save %sp, %g1, %sp
	 */
	set	0x9DE3B, %l5 /* (save %sp, -imm13, %sp) >> 12 */
	srl	%l7, 12, %l6
	cmp	%l6, %l5
	bne	uwin_check_g1
	 /* put imm13 in %l6, sign-extending */
	 sll	%l7, 19, %l6
	sra	%l6, 19, %l6

	SHIFT_O2I

	add	%sp, %l6, %sp

	jmp	%g5
	 jmp	%g5 + 4
	  nop

uwin_check_g1:
	set	0x9DE38001, %l5 /* save %sp, %g1, %sp */
	cmp	%l7, %l5
	bne	overflow_bail
	 /* we are hosed if cmp fails, so just do it */
	 SHIFT_O2I 

	add	%sp, %g1, %sp

	jmp	%g5
	 jmp	%g5 + 4
	  nop

overflow_bail:
	mov	%l7, %o0
	mov	%y, %o1
	ta	SOFTWARE_TRAP(T_EMUERR)
	 nop

.section .uwin_underflow, "ax", @progbits
.globl uwin_underflow
uwin_underflow:
	/* Can use locals, util LOAD_WINDOW */
	/* Read inst op */
	mov	%y, %l6
	ld	[%l6], %l7

	/* We only handle "restore". */
	set	0x81E80000, %l6 /* "restore" machine code */
	cmp	%l6, %l7
	bne	underflow_bail
	 mov	%i0, %o0 /* return value */
	mov	%i1, %o1
	mov	%i2, %o2
	mov	%i3, %o3
	mov	%i4, %o4
	mov	%i5, %o5
	mov	%i6, %o6 /* %fp -> %sp */
	mov	%i7, %o7
	
	LOAD_WINDOW(sp)
	
	jmp	%g5
	 jmp	%g5 + 4
	  nop
	
underflow_bail:
	mov	%l7, %o0
	mov	%y, %o1
	ta	SOFTWARE_TRAP(T_EMUERR)
	 nop
