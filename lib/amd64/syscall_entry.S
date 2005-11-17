#include <machine/asm.h>
#include <machine/trapcodes.h>

ENTRY(syscall)
	int	$T_SYSCALL
	ret

