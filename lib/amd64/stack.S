#include <machine/asm.h>

ENTRY(stack_switch)
	movq	$0xfffffffffffffff0, %rax
	andq	%r8, %rax
	movq	%r8, %rsp

	// terminate _Unwind_Backtrace; also provides correct 16-byte alignment
	pushq	$0
	jmp	*%r9

