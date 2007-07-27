#include <machine/asm.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	ta SOFTWARE_TRAP(T_SYSCALL)
	 nop
	retl
	 nop
