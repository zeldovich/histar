#include <machine/memlayout.h>
	
	.text
.globl	trapframe_pop
trapframe_pop:
	pushw	tf_cs(%eax)
	pushl	$(UKSWITCH + 1)
	ljmp	*(%esp)

/*
 * The switch code gets copied to USWITCH by the kernel.
 */
.globl	nacl_switch	
nacl_switch:
	hlt			/* one byte */

	movl	%eax,%esp

	movw	tf_ds(%esp),%ax	
	movw	%ax,(UKSCRATCH)
	movl	tf_eip(%esp),%eax	
	movl	%eax,(UKSCRATCH + 4)
	
	movw	tf_es(%esp),%es
		
	movl	tf_eax(%esp),%eax
	movl	tf_ebx(%esp),%ebx
	movl	tf_ecx(%esp),%ecx
	movl	tf_edx(%esp),%edx
	movl	tf_esi(%esp),%esi
	movl	tf_edi(%esp),%edi
	movl	tf_ebp(%esp),%ebp

	addl	$tf_eflags,%esp
	popfl
	lss	(%esp),%esp
	movw	(UKSCRATCH),%ds
	jmp	*(UKSCRATCH + 4)
.globl	nacl_switch_end	
nacl_switch_end:

/*
 * The usyscall code gets copied to USYSCALL by the kernel.
 */
.globl	nacl_usyscall	
nacl_usyscall:
	movw	UKINFO,%ds

	movl	%eax,(UKSCRATCH)
	movl	%esp,(UKSCRATCH + 4) 
	lss	kstack_ptr, %esp

	pushw	%gs
	pushw	%ss
	movl	(UKSCRATCH + 4),%eax
	pushl	%eax
	pushfl
	pushw	%fs
	pushw	%cs
	pushl	(%eax)		/* return eip sitting on caller stack */
	pushl	$0
	pushl	(UKSCRATCH)
	pushl	%edx
	pushw	%es
	pushw	%ds
	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%ecx
	pushl	%ebx

	movl	%esp,%eax	/* Trapframe arg to syscall_handler */
	xorl	%ebp,%ebp	
	pushl	$0
	ljmp	*(syscall_target)
.globl	nacl_usyscall_end	
nacl_usyscall_end:
