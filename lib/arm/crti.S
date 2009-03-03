/* From FreeBSD: */

.section .init
.global	_init
.type	_init, %function
_init:
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

.section .fini
.global	_fini
.type	_fini, %function
	mov	ip, sp
	stmdb	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4
