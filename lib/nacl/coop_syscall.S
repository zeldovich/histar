#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <machine/memlayout.h>
#include <inc/syscall_asm.h>
#include <inc/cooperate.h>

#define SYSCALL_ARGS	UTLSTOP-PGSIZE

ENTRY(cooperate_syscall)
	/*
	 * The address of (struct coop_syscall_args) is in %eax.
	 */
	movl	$SYSCALL_ARGS, %edx

	movl	0x00(%eax), %esp	// syscall number
	popl	%ecx

	movl	0x04(%eax), %esp	// arg1
	popl	0x00(%edx)
	popl	0x04(%edx)

	movl	0x08(%eax), %esp	// arg2
	popl	0x08(%edx)
	popl	0x0c(%edx)

	movl	0x0c(%eax), %esp	// arg3
	popl	0x10(%edx)
	popl	0x14(%edx)

	movl	0x10(%eax), %esp	// arg4
	popl	0x18(%edx)
	popl	0x1c(%edx)

	movl	0x14(%eax), %esp	// arg5
	popl	0x20(%edx)
	popl	0x24(%edx)

	movl	0x18(%eax), %esp	// arg6
	popl	0x28(%edx)
	popl	0x2c(%edx)

	movl	0x1c(%eax), %esp	// arg7
	popl	0x30(%edx)
	popl	0x34(%edx)

	/*
	 * Now that we've copied all the system call arguments,
	 * execute the actual system call.
	 */
	movl	%ecx, %eax
	movl	$SYSCALL_ARGS, %edx

	int	$T_SYSCALL
	movl	%eax, COOP_RETVAL
	movl	%edx, COOP_RETVAL+4
	movl	$1, COOP_STATUS

	movl	$SYS_sync_wakeup, %eax
	movl	$SYSCALL_ARGS, %edx
	movl	$COOP_STATUS, 0(%edx)
	movl	$0, 4(%edx)
	int	$T_SYSCALL

	movl	$SYS_self_halt, %eax
	int	$T_SYSCALL
1:	jmp	1b

ENTRY(cooperate_syscall_end)

