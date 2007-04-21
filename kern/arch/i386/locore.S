
#include <machine/pmap.h>

/*
 * Boot sequence
 */
	.text
.globl	start
start:	.code32
	movw	$(0x0400 + '7'), (0xb8010)
	movl	%eax,%ebp			// Save multiboot magic

	movl	%cr4,%eax			// Enable global pages
	orl	$(CR4_PGE|CR4_PSE|CR4_OSFXSR|CR4_OSX),%eax
	movl	%eax,%cr4

	movl	$RELOC(bootpd),%eax		// Load CR3
	movl	%eax,%cr3

	movl	$RELOC(PD_SKIP + gdtdesc),%eax	// Load GDT
	lgdt	(%eax)

	movl	$(RELOC(farptr)),%ecx		// Far pointer
	movl	%cr0,%eax			// Enable paging
	orl	$(CR0_PE|CR0_PG|CR0_WP|CR0_AM|CR0_TS|CR0_MP|CR0_NE),%eax
	movl	%eax,%cr0
	movw	$(0x0400 + '8'), (0xb8012)

	ljmp	*(%ecx)
almost:
	movw	$(0x0500 + '9'), (0xb8014)

	movw	$GD_KD, %ax
	movw	%ax, %ss
	movw	%ax, %ds
	movw	%ax, %es

	movl	$(kstack + KSTACK_SIZE),%esp	// set %esp to top of stack
	movl	%ebp,%eax			// restore & pass multiboot magic
	movl	%ebx,%edx			// pass multiboot pointer
	xorl	%ebp,%ebp			// clear frame pointer
	movl	$init,%ebx
	call	*%ebx				// call by absolute address
1:	jmp	1b				// init should not return

farptr: .long	RELOC(almost)
	.word	GD_KT


/*
 * Trap handlers
 */


// These trap entry stubs must fit within 16 bytes of code
.globl	trap_noec_entry_stub
trap_noec_entry_stub:
	// %esp is pointing at tf_eip
	pushl	%eax		// adjust %esp
.globl	trap_ec_entry_stub
trap_ec_entry_stub:
	// %esp is pointing at tf_err
	pushl	%eax		// eax into tf_eax
	movl	$trap_entry, %eax
	call	*%eax		// eip into tf__trapentry_eip

.globl	trap_entry
trap_entry:
	subl	$tf__trapentry_eip,%esp

	// %eax was already saved by the trampoline stub
	movl	%ebx,tf_ebx(%esp)
	movl	%ecx,tf_ecx(%esp)
	movl	%edi,tf_edi(%esp)
	movl	%esi,tf_esi(%esp)
	movl	%ebp,tf_ebp(%esp)
	movw	%ds,tf_ds(%esp)
	movw	%es,tf_es(%esp)

	movw	$GD_KD, %ax
	movw	%ax, %ds
	movw	%ax, %es

	movl	%esp,%eax	    // first arg: Trapframe
	xchgl	%edx,tf_edx(%esp)   // %edx and _trapentry_eip are union'd
	jmp	trap_handler

.globl	trapframe_pop
trapframe_pop:
	movl	%eax,%esp

	movw	tf_ds(%esp),%ds
	movw	tf_es(%esp),%es

	movl	tf_eax(%esp),%eax
	movl	tf_ebx(%esp),%ebx
	movl	tf_ecx(%esp),%ecx
	movl	tf_edx(%esp),%edx
	movl	tf_esi(%esp),%esi
	movl	tf_edi(%esp),%edi
	movl	tf_ebp(%esp),%ebp

	addl	$tf_eip,%esp
	iret
