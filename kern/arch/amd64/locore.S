
#include <machine/pmap.h>

/*
 * Boot sequence
 */
	.text
.globl	start
start:	.code32
	movl	%cr4,%eax			// Enable PAE
	orl	$(CR4_PAE|CR4_PGE),%eax
	movl	%eax,%cr4

	movl	$RELOC(bootpml4),%eax		// Load CR3
	movl	%eax,%cr3

	movl	$EFER,%ecx			// Set long-mode enable bit
	rdmsr
	orl	$(EFER_SCE|EFER_LME|EFER_NXE),%eax
	wrmsr

	movl	$RELOC(PD_SKIP + gdtdesc),%eax	// Load GDT
	lgdt	(%eax)

	movl	$(RELOC (farptr)),%edx		// Far porter to inlong
	movl	%cr0,%eax			// Enable paging
	orl	$(CR0_PE|CR0_PG|CR0_WP|CR0_AM),%eax
	movl	%eax,%cr0
	ljmp	*(%edx)

inlong:	.code64
	movabsq	$(KSTACKTOP),%rsp		// set %rsp to top of stack
	xorl	%ebp,%ebp			// clear frame pointer
	movq	$init,%r11			// call init function
	call	*%r11
1:	jmp	1b				// shouldn't get here

farptr: .long	RELOC(inlong)
	.word	GD_KT


/*
 * Trap handlers
 */
	.macro	callersave
	movq	%rax,tf_rax(%rsp)
	movq	%rcx,tf_rcx(%rsp)
	movq	%rdx,tf_rdx(%rsp)
	movq	%rsi,tf_rsi(%rsp)
	movq	%rdi,tf_rdi(%rsp)
	movq	%r8,tf_r8(%rsp)
	movq	%r9,tf_r9(%rsp)
	movq	%r10,tf_r10(%rsp)
	movq	%r11,tf_r11(%rsp)
	.endm
	.macro	calleesave
	movq	%rbx,tf_rbx(%rsp)
	movq	%rbp,tf_rbp(%rsp)
	movq	%r12,tf_r12(%rsp)
	movq	%r13,tf_r13(%rsp)
	movq	%r14,tf_r13(%rsp)
	movq	%r15,tf_r15(%rsp)
	.endm

	.macro	callerrestore
	movq	tf_rax(%rsp),%rax
	movq	tf_rcx(%rsp),%rcx
	movq	tf_rdx(%rsp),%rdx
	movq	tf_rsi(%rsp),%rsi
	movq	tf_rdi(%rsp),%rdi
	movq	tf_r8(%rsp),%r8
	movq	tf_r9(%rsp),%r9
	movq	tf_r10(%rsp),%r10
	movq	tf_r11(%rsp),%r11
	.endm
	.macro	calleerestore
	movq	tf_rbx(%rsp),%rbx
	movq	tf_rbp(%rsp),%rbp
	movq	tf_r12(%rsp),%r12
	movq	tf_r13(%rsp),%r13
	movq	tf_r13(%rsp),%r14
	movq	tf_r15(%rsp),%r15
	.endm

.macro	trap	number,errcode,handler
	.align	16
	.globl	X\handler
X\handler:
.ifne	\errcode
	subq	$tf_err,%rsp
.else
	subq	$tf_rip,%rsp
.endif
	callersave
	calleesave
	movq	%rsp,%rdi
	movq	$\errcode,%rsi
	call	\handler
	callerrestore
	calleerestore
	addq	$tf_rip,%rsp
	iret
	.endm

	trap	0,1,unknownec
	trap	0,0,unknown

.globl	trapframe_pop
trapframe_pop:
	movq	%rdi,%rsp
	callerrestore
	calleerestore
	addq	$tf_rip,%rsp
	iretq

// Local Variables:
// comment-start: "// "
// comment-char: ?/
// End:
