.section .init
.global	_init
.type	_init, %function
_init:
	save	%sp, -104, %sp

.section .fini
.global	_fini
.type	_fini, %function
_fini:
	save	%sp, -104, %sp

