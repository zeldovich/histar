#include <machine/asm.h>

ENTRY(stack_switch)
	movq	$0xfffffffffffffff0, %rax
	andq	%r8, %rax
	movq	%r8, %rsp

	// terminate _Unwind_Backtrace; also provides correct 16-byte alignment
	// abi-0.96.pdf: "In other words, the value (%rsp-8) is always a multiple
	// of 16 when control is transferred to the function entry point."
	pushq	$0
	jmp	*%r9

