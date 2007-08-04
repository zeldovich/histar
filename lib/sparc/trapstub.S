#include <machine/asm.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <obj/lib/utrapasm.h>


ENTRY(utrap_entry_asm)
	call	utrap_entry
	 mov	%sp, %o0

.section .utrap_masked, "ax", @progbits
.globl utrap_ret
.type utrap_ret, %function
utrap_ret:
	mov	%o0, %g7
	ld	[%g7 + 136], %g6
	mov	%g6, %y

	LOAD_INS(g7 + TF_I0)
	LOAD_LOCALS(g7 + TF_L0)
	LOAD_OUTS(g7 + TF_O0)

	ld [%g7 + TF_G1], %g1
	ldd [%g7 + TF_G2], %g2
	ldd [%g7 + TF_G4], %g4

	ld	[%g7 + 128], %g6
	ld	[%g7 + 132], %g7

	jmp	%g6
	 jmp	%g7
	  nop
