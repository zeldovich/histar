
.text
.globl _start
_start:
	movq	$0, %rdi	// argc
	movq	$0, %rsi	// argv

	call	libmain
1:	jmp	1b

