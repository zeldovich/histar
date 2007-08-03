#include <machine/asm.h>
#include <machine/pmap.h>
#include <obj/lib/utrapasm.h>


ENTRY(utrap_entry_asm)
	mov	%sp, %o0
	call	utrap_entry
	 nop

.section .utrap_masked, "ax", @progbits
.globl utrap_ret
.type utrap_ret, %function
utrap_ret:
	ba utrap_ret
	 nop
