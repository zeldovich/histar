.section .init
.global	_init
.type	_init, %function
	ret
	restore
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	ret
	restore
.size _fini, .-_fini

