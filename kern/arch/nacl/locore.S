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

	lss	USCRATCH, %esp
	jmp	*(USCRATCH + 8)
.globl	nacl_springboard_end	
nacl_springboard_end:
