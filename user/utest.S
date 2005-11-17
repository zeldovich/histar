
.code64
.globl	_start
_start:
	movq	$0xd001, %rax
	movq	$0xd002, %rbx
	movq	$0xd003, %rcx
	movq	$0xd004, %rdx
	movq	$0xd005, %rdi
	movq	$0xd006, %rsi
	movq	$0xd007, %rbp
	movq	$0xd008, %r8
	movq	$0xd009, %r9
	movq	$0xd00a, %r10
	movq	$0xd00b, %r11
	movq	$0xd00c, %r12
	movq	$0xd00d, %r13
	movq	$0xd00e, %r14
	movq	$0xd00f, %r15
	int	$0x30

	movq	$0xdeadbeef, %rax
	int	$0x30

loop:
	jmp	loop

