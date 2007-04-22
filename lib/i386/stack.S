#include <machine/asm.h>

ENTRY(stack_curptr)
	movl	%esp, %eax
	ret

ENTRY(stack_switch)
	// Stack layout:
	//  %esp	Return address
	//  %esp + 4	64-bit arg #0
	//  %esp + 12	64-bit arg #1
	//  %esp + 20	64-bit arg #2
	//  %esp + 28	64-bit arg #3
	//  %esp + 36	Target stack top
	//  %esp + 40	Target %eip

	// Figure out the target stack address
	movl	36(%esp), %ebx
	subl	$32, %ebx

	// Call memcpy to copy the 64-bit args onto the target stack
	leal	4(%esp), %eax
	pushl	$32
	pushl	%eax
	pushl	%ebx
	call	memcpy
	addl	$12, %esp

	// Switch to target stack
	movl	40(%esp), %eax
	movl	%ebx, %esp

	// Terminate stack unwinding
	movl	$0, %ebp
	pushl	$0
	jmp	*%eax

