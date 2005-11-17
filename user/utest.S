
.code64
.globl	_start
_start:
	movq	$0xd00deadbeef, %rax
	call	*%rax

