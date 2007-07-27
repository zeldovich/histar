#include <machine/asm.h>
#include <machine/mmu.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	mov	%sp, %g1
	save	%sp, -STACKFRAME_SZ, %sp

	ld	[%g1 + 92], %l0
	ld	[%g1 + 96], %l1
	ld	[%g1 + 100], %l2
	ld	[%g1 + 104], %l3
	ld	[%g1 + 108], %l4
	ld	[%g1 + 112], %l5	
	ld	[%g1 + 116], %l6							
	ld	[%g1 + 120], %l7

	ld	[%g1 + 124], %o0
	
	ta SOFTWARE_TRAP(T_SYSCALL)
	 nop

	ret
	 restore
