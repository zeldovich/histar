.section .init
.global	_init
.type	_init, %function
	ba _init
	 nop
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	ba _fini
	 nop
.size _fini, .-_fini

