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
	addl    $8, %esp	/* springboard addr, cs */

	movl	%eax,%esp
	movl	tf_esp(%esp),%eax	
	movl	%eax, UKSCRATCH
	movl	tf_ss(%esp),%eax	
	movl	%eax, UKSCRATCH + 4	
	movl	tf_eip(%esp),%eax	
	movl	%eax, UKSCRATCH + 8 
	
	movw	tf_es(%esp),%es
		
	movl	tf_eax(%esp),%eax
	movl	tf_ebx(%esp),%ebx
	movl	tf_ecx(%esp),%ecx
	movl	tf_edx(%esp),%edx
	movl	tf_esi(%esp),%esi
	movl	tf_edi(%esp),%edi
	movl	tf_ebp(%esp),%ebp
		
	movw	tf_ds(%esp),%ds

	// XXX eflags?
	
	lss	UKSCRATCH, %esp
	jmp	*(UKSCRATCH + 8)
.globl	nacl_switch_end	
nacl_switch_end:

/*
 * The usyscall code gets copied to USYSCALL by the kernel.
 */
.globl	nacl_usyscall	
nacl_usyscall:
	movw	UKINFO,%ds
	
	movl	%ebx,(UKSCRATCH + tf_ebx)
	movl	%ecx,(UKSCRATCH + tf_ecx)
	movl	%edi,(UKSCRATCH + tf_edi)
	movl	%esi,(UKSCRATCH + tf_esi)
	movl	%ebp,(UKSCRATCH + tf_ebp)
	movw	%ds,(UKSCRATCH + tf_ds)
	movw	%es,(UKSCRATCH + tf_es)

	movl	%edx,(UKSCRATCH + tf_edx)
	movl	%eax,(UKSCRATCH + tf_eax)

	movl	(%esp), %eax
	movl	%eax,(UKSCRATCH + tf_eip) 
	
	movw	%cs, %ax
	movw	%ax,(UKSCRATCH + tf_cs)
	movw	%fs, %ax
	movw	%ax,(UKSCRATCH + tf_fs)
	addl	$4, %esp
	movl	%esp,(UKSCRATCH + tf_esp)
	movw	%ss, %ax
	movw	%ax,(UKSCRATCH + tf_ss)
	movw	%gs, %ax
	movw	%ax,(UKSCRATCH + tf_gs)
	
	lss	kstack_ptr, %esp
	xorl	%ebp, %ebp
	pushl	$0
	ljmp	syscall_target
.globl	nacl_usyscall_end	
nacl_usyscall_end:
