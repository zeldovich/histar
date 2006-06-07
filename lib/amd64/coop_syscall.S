#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <inc/syscall_asm.h>

ENTRY(cooperate_syscall)
	int	$T_SYSCALL

	movq	$SYS_self_halt, %rdi
	int	$T_SYSCALL
1:	jmp	1b

ENTRY(cooperate_syscall_end)

