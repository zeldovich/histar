#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <inc/syscall_asm.h>
#include <inc/cooperate.h>

ENTRY(cooperate_syscall)
	ba cooperate_syscall
	 nop

ENTRY(cooperate_syscall_end)

