#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <machine/memlayout.h>
	
ENTRY(syscall)
	.cfi_startproc
	movl	4(%esp), %eax
	leal	8(%esp), %edx
	call	USYSCALL
	ret
	.cfi_endproc

