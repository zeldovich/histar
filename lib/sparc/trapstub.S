#include <machine/asm.h>
#include <machine/pmap.h>
#include <obj/lib/utrapasm.h>


ENTRY(utrap_entry_asm)
	ba utrap_entry_asm
	 nop

ENTRY(utrap_ret)
	ba utrap_ret
	 nop
