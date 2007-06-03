#include <machine/asm.h>

ENTRY(jos_setjmp)
	movl	4(%esp), %ecx	// jos_jmp_buf

	movl	0(%esp), %edx	// %eip as pushed by call
	movl	%edx,  0(%ecx)

	leal	4(%esp), %edx	// where %esp will point when we return
	movl	%edx,  4(%ecx)

	movl	%ebp,  8(%ecx)
	movl	%ebx, 12(%ecx)
	movl	%esi, 16(%ecx)
	movl	%edi, 20(%ecx)

	movl	$0, %eax
	ret

ENTRY(jos_longjmp)
	movl	 4(%esp), %edx	// jos_jmp_buf
	movl	 8(%esp), %eax	// return value

	movl	 0(%edx), %ecx	// %eip
	movl	 4(%edx), %esp
	movl	 8(%edx), %ebp
	movl	12(%edx), %ebx
	movl	16(%edx), %esi
	movl	20(%edx), %edi

	cmpl	$0, %eax	// never return 0
	jnz	1f
	movl	$1, %eax
1:
	jmp	*%ecx

