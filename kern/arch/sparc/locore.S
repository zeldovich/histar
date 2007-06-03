.text
.globl	start
start:
	mov	%l0, %l1
1:	jmp 1b

trap_return:
	jmpl	%r17, %r0	/* old PC */
	rett	%r18		/* old nPC */

