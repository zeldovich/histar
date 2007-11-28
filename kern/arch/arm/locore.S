#include <machine/asm.h>

.text
.globl start
start:
	b	1f
	.rept 8
	nop
	.endr

	.long	0x016f2818
	.long	start
	.long	_edata

1:
	b	1b

