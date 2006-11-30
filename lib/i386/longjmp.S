#include <machine/asm.h>

ENTRY(jos_setjmp)
	movl	4(%esp), %ecx	// jos_jmp_buf

	movl	0(%esp), %edx	// %eip as pushed by call
	movl	%edx,  0(%ecx)
	movl	%ecx,  4(%ecx)	// %ecx is where %esp will point when we return
	movl	%ebp,  8(%ecx)
	movl	%ebx, 12(%ecx)
	movl	%esi, 16(%ecx)
	movl	%edi, 20(%ecx)

	movl	$0, %eax
	ret

ENTRY(jos_longjmp)
	movl	 4(%esp), %ecx	// jos_jmp_buf
	movl	 8(%esp), %eax	// return value

	movl	 0(%ecx), %edx	// %eip
	movl	 4(%ecx), %esp
	movl	 8(%ecx), %ebp
	movl	12(%ecx), %ebx
	movl	16(%ecx), %esi
	movl	20(%ecx), %edi

	cmpl	$0, %eax	// never return 0
	jnz	1f
	movl	$1, %eax
1:
	jmp	*%edx

