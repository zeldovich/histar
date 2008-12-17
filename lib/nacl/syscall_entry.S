#include <machine/asm.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	.cfi_startproc
	movl	4(%esp), %eax
	leal	8(%esp), %edx
	int	$T_SYSCALL
	ret
	.cfi_endproc

