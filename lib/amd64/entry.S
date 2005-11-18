
.text
.globl _start
_start:
	movq	$0, %rdi	// argc
	movq	$0, %rsi	// argv

	call	libmain

	// libmain shouldn't return -- try to crash if it does
	movq	$0, %rax
	call	*%rax

