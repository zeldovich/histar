.section .init
.global	_init
.type	_init, %function
_init:
	subq	$8, %rsp

.section .fini
.global	_fini
.type	_fini, %function
_fini:
	subq	$8, %rsp

