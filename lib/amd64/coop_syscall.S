#include <machine/asm.h>
#include <machine/trapcodes.h>
#include <inc/syscall_asm.h>
#include <inc/cooperate.h>

ENTRY(cooperate_syscall)
	/*
	 * The address of (struct coop_syscall_args) is in %rdi.
	 */
	movq	%rdi, %rax

	movq	0x00(%rax), %rsp
	pop	%rdi

	movq	0x08(%rax), %rsp
	pop	%rsi

	movq	0x10(%rax), %rsp
	pop	%rdx

	movq	0x18(%rax), %rsp
	pop	%rcx

	movq	0x20(%rax), %rsp
	pop	%r8

	movq	0x28(%rax), %rsp
	pop	%r9

	movq	0x30(%rax), %rsp
	pop	%r10

	movq	0x38(%rax), %rsp
	pop	%r11

	/*
	 * Now that we've copied all the system call arguments,
	 * execute the actual system call.
	 */

	int	$T_SYSCALL
	movq	%rax, COOP_RETVAL
	movq	$1, COOP_STATUS

	movq	$SYS_sync_wakeup, %rdi
	movq	$COOP_STATUS, %rsi
	int	$T_SYSCALL

	movq	$SYS_self_halt, %rdi
	int	$T_SYSCALL
1:	jmp	1b

ENTRY(cooperate_syscall_end)

