#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <machine/memlayout.h>
	
ENTRY(syscall)
	.cfi_startproc
	movl	4(%esp), %eax
	leal	8(%esp), %edx
	call	UKSYSCALL
	ret
	.cfi_endproc
