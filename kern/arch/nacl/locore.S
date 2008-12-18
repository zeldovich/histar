#include <machine/memlayout.h>
	
	.text
.globl	trapframe_pop
trapframe_pop:
	movl	%eax,%esp

	push	tf_cs(%esp)
	push	$(USPRING + 1)
	ljmp	*(%esp)

/*
 * The springboard code gets copied to USPRING by the kernel.
 */
.globl	nacl_springboard	
nacl_springboard:
	hlt			/* one byte */
	addl    $8, %esp	/* springboard addr, cs */

	movl	tf_esp(%esp),%eax	
	movl	%eax, USCRATCH
	movl	tf_ss(%esp),%eax	
	movl	%eax, USCRATCH + 4	
	movl	tf_eip(%esp),%eax	
	movl	%eax, USCRATCH + 8 
	
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
	
	lss	USCRATCH, %esp
	jmp	*(USCRATCH + 8)
.globl	nacl_springboard_end	
nacl_springboard_end:

/*
 * The usyscall code gets copied to USYSCALL by the kernel.
 */
.globl	nacl_usyscall	
nacl_usyscall:
	movl	%ebx,(USCRATCH + tf_ebx)
	movl	%ecx,(USCRATCH + tf_ecx)
	movl	%edi,(USCRATCH + tf_edi)
	movl	%esi,(USCRATCH + tf_esi)
	movl	%ebp,(USCRATCH + tf_ebp)
	movw	%ds,(USCRATCH + tf_ds)
	movw	%es,(USCRATCH + tf_es)

	movl	%edx,(USCRATCH + tf_edx)
	movl	%eax,(USCRATCH + tf_eax)

	movl	(%esp), %eax
	movl	%eax,(USCRATCH + tf_eip) 
	
	movl	%cs, %eax
	movl	%eax,(USCRATCH + tf_cs)
	movl	%fs, %eax
	movl	%eax,(USCRATCH + tf_fs)
	addl	$4, %esp
	movl	%esp,(USCRATCH + tf_esp)
	movl	%ss, %eax
	movl	%eax,(USCRATCH + tf_ss)
	movl	%gs, %eax
	movl	%eax,(USCRATCH + tf_gs)
	
	// XXX should change %esp, zero %edp
	// XXX eflags?
	// XXX linker script will fix this hack
	mov	$syscall_handler,%ebx
	call	*%ebx
.globl	nacl_usyscall_end	
nacl_usyscall_end:
