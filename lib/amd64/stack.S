#include <machine/asm.h>

ENTRY(stack_switch)
	movq	%r8, %rsp
	pushq	$0		// terminate _Unwind_Backtrace
	jmp	*%r9

