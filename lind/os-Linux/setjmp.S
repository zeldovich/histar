#
# arch/x86_64/setjmp.S
#
# setjmp/longjmp for the x86-64 architecture
#

#
# The jmp_buf is assumed to contain the following, in order:
#	%rbx
#	%rsp (post-return)
#	%rbp
#	%r12
#	%r13
#	%r14
#	%r15
#	<return address>
#

	.text
	.align 4
	.globl lind_setjmp
	.type lind_setjmp, @function
lind_setjmp:
	pop  %rsi			# Return address, and adjust the stack
	xorl %eax,%eax			# Return value
	movq %rbx,(%rdi)
	movq %rsp,8(%rdi)		# Post-return %rsp!
	push %rsi			# Make the call/return stack happy
	movq %rbp,16(%rdi)
	movq %r12,24(%rdi)
	movq %r13,32(%rdi)
	movq %r14,40(%rdi)
	movq %r15,48(%rdi)
	movq %rsi,56(%rdi)		# Return address
	ret

	.size lind_setjmp,.-lind_setjmp

	.text
	.align 4
	.globl lind_longjmp
	.type lind_longjmp, @function
lind_longjmp:
	movl %esi,%eax			# Return value (int)
	movq (%rdi),%rbx
	movq 8(%rdi),%rsp
	movq 16(%rdi),%rbp
	movq 24(%rdi),%r12
	movq 32(%rdi),%r13
	movq 40(%rdi),%r14
	movq 48(%rdi),%r15
	jmp *56(%rdi)

	.size lind_longjmp,.-lind_longjmp
