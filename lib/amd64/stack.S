#include <machine/asm.h>

ENTRY(stack_switch)
	movq	%r8, %rsp
	jmp	*%r9

