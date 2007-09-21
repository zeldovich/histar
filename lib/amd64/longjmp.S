#include <machine/asm.h>

ENTRY(jos_setjmp)
	popq	%rax		// %rip pushed by call
	movq	%rax,  0(%rdi)
	movq	%rsp,  8(%rdi)
	movq	%rbp, 16(%rdi)
	movq	%rbx, 24(%rdi)
	movq	%r12, 32(%rdi)
	movq	%r13, 40(%rdi)
	movq	%r14, 48(%rdi)
	movq	%r15, 56(%rdi)

	pushq	%rax
	movq	$0, %rax
	ret

ENTRY(jos_longjmp)
	movq	 0(%rdi), %rax	// %rip
	movq	 8(%rdi), %rsp
	movq	16(%rdi), %rbp
	movq	24(%rdi), %rbx
	movq	32(%rdi), %r12
	movq	40(%rdi), %r13
	movq	48(%rdi), %r14
	movq	56(%rdi), %r15

	pushq	%rax		// push %rip for ret
	movq	%rsi, %rax
	ret

