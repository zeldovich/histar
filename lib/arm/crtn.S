/* From FreeBSD: */

.section .init
.global	_init
.type	_init, %function
	ldmea	fp, {fp, sp, pc}
#ifdef __thumb__
	bx	lr
#else
	mov	pc, lr
#endif
.size _init, .-_init

.section .fini
.global	_fini
.type	_fini, %function
	ldmea	fp, {fp, sp, pc}
#ifdef __thumb__
	bx	lr
#else
	mov	pc, lr
#endif
.size _fini, .-_fini
