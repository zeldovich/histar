.section .init
.global	_init
.type	_init, %function
_init:
	ba _init
	 nop

.section .fini
.global	_fini
.type	_fini, %function
	ba _fini
	 nop

