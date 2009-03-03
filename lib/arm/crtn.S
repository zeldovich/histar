/* From FreeBSD: */

.section .init
.global	_init
.type	_init, %function
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	ldmea	fp, {fp, sp, pc}
	mov	pc, lr
.size _fini, .-_fini
