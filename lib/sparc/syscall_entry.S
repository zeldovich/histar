#include <machine/asm.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	ba syscall
	 nop

