#include <machine/asm.h>

ENTRY(_start)
	call _init
	call libmain

.section .init
	.globl _init
	.type  _init, @function
_init:

