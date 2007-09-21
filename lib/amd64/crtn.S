.section .init
.global	_init
.type	_init, %function
	addq $8, %rsp
	ret
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	addq $8, %rsp
	ret
.size _fini, .-_fini

