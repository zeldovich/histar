.section .init
.global	_init
.type	_init, %function
	popl	%ebp
	ret
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	popl	%ebp
	ret
.size _fini, .-_fini

