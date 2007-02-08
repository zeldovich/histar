#include <machine/asm.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	.cfi_startproc
	// Move additional arguments in place
	movq	 8(%rsp), %r10
	movq	16(%rsp), %r11

	int	$T_SYSCALL
	ret
	.cfi_endproc

