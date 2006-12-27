
#include <machine/pmap.h>

/*
 * Boot sequence
 */
	.text
.globl	start
start:	.code32
	movl	%eax,%ebp			// Save multiboot magic

	movl	%cr4,%eax			// Enable PAE
	orl	$(CR4_PAE|CR4_PGE|CR4_OSFXSR|CR4_OSX),%eax
	movl	%eax,%cr4

	movl	$RELOC(bootpml4),%eax		// Load CR3
	movl	%eax,%cr3

	movl	$EFER,%ecx			// Set long-mode enable bit
	rdmsr
	orl	$(EFER_LME|EFER_NXE),%eax
	wrmsr

	movl	$RELOC(PD_SKIP + gdtdesc),%eax	// Load GDT
	lgdt	(%eax)

	movl	$(RELOC (farptr)),%ecx		// Far porter to inlong
	movl	%cr0,%eax			// Enable paging
	orl	$(CR0_PE|CR0_PG|CR0_WP|CR0_AM|CR0_TS|CR0_MP|CR0_NE),%eax
	movl	%eax,%cr0
	ljmp	*(%ecx)

inlong:	.code64
	movabsq	$(KSTACKTOP),%rsp		// set %rsp to top of stack
	movl	%ebp,%edi			// restore & pass multiboot magic
	movl	%ebx,%esi			// pass multiboot pointer
	xorq	%rbp,%rbp			// clear frame pointer
	movq	$init,%r11			// call init function
	call	*%r11
1:	jmp	1b				// shouldn't get here

farptr: .long	RELOC(inlong)
	.word	GD_KT


/*
 * Trap handlers
 */


// These trap entry stubs must fit within 16 bytes of code
.globl	trap_noec_entry_stub
trap_noec_entry_stub:
	// %rsp is pointing at tf_rip
	pushq	%rax		// adjust %rsp
.globl	trap_ec_entry_stub
trap_ec_entry_stub:
	// %rsp is pointing at tf_err
	pushq	%rax		// rax into tf_rax
	movq	$trap_entry, %rax
	call	*%rax		// rip into tf__trapentry_rip

.globl	trap_entry
trap_entry:
	subq	$tf__trapentry_rip,%rsp

	// %rax was already saved by the trampoline stub
	movq	%rbx,tf_rbx(%rsp)
	movq	%rcx,tf_rcx(%rsp)
	movq	%rdx,tf_rdx(%rsp)
	xchgq	%rsi,tf_rsi(%rsp) // %rsi and _trapentry_rip are union'd
	movq	%rdi,tf_rdi(%rsp)
	movq	%rbp,tf_rbp(%rsp)
	movq	%r8,tf_r8(%rsp)
	movq	%r9,tf_r9(%rsp)
	movq	%r10,tf_r10(%rsp)
	movq	%r11,tf_r11(%rsp)
	movq	%r12,tf_r12(%rsp)
	movq	%r13,tf_r13(%rsp)
	movq	%r14,tf_r14(%rsp)
	movq	%r15,tf_r15(%rsp)

	movw	%ds,tf_ds(%rsp)
	movw	%es,tf_es(%rsp)
	movw	%fs,tf_fs(%rsp)
	movw	%gs,tf_gs(%rsp)

	movq	%rsp,%rdi
	// %rsi was exchanged earlier with the trampoline's %rip
	jmp	trap_handler

.globl	trapframe_pop
trapframe_pop:
	movq	%rdi,%rsp

	movq	tf_rax(%rsp),%rax
	movq	tf_rbx(%rsp),%rbx
	movq	tf_rcx(%rsp),%rcx
	movq	tf_rdx(%rsp),%rdx
	movq	tf_rsi(%rsp),%rsi
	movq	tf_rdi(%rsp),%rdi
	movq	tf_rbp(%rsp),%rbp
	movq	tf_r8(%rsp),%r8
	movq	tf_r9(%rsp),%r9
	movq	tf_r10(%rsp),%r10
	movq	tf_r11(%rsp),%r11
	movq	tf_r12(%rsp),%r12
	movq	tf_r13(%rsp),%r13
	movq	tf_r14(%rsp),%r14
	movq	tf_r15(%rsp),%r15

	movw	tf_ds(%rsp),%ds
	movw	tf_es(%rsp),%es
	movw	tf_fs(%rsp),%fs
	movw	tf_gs(%rsp),%gs

	addq	$tf_rip,%rsp
	iretq

// Local Variables:
// comment-start: "// "
// comment-char: ?/
// End:
