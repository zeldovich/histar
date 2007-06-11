#include <machine/asm.h>
#include <machine/pmap.h>
#include <obj/lib/utrapasm.h>

/*
 * When we upgrade to binutils 2.17, we should start using .cfi_signal_frame:
 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=26208
 */

#define CFI_UTF_OFFSETS		\
	.cfi_offset eax, 0*4;	\
	.cfi_offset ebx, 1*4;	\
	.cfi_offset ecx, 2*4;	\
	.cfi_offset edx, 3*4;	\
				\
	.cfi_offset esi, 4*4;	\
	.cfi_offset edi, 5*4;	\
	.cfi_offset ebp, 6*4;	\
	.cfi_offset esp, 7*4;	\
				\
	.cfi_offset eip, 8*4;

ENTRY(utrap_entry_asm)
	.cfi_startproc simple

	// Our entire CPU state is saved in a UTrapframe right at %esp
	.cfi_def_cfa esp, 0

	// Tell DWARF2 about our saved registers; this is UTrapframe layout
	CFI_UTF_OFFSETS

	// Pass the address of the UTrapframe to utrap_entry()
	movl	%esp, %eax

	// Call instead of jmp, because we want the call stack to point back to
	// this function, so that the unwinder sees our register remapping.
	call	utrap_entry
	.cfi_endproc

ENTRY(utrap_chain_dwarf2)
	.cfi_startproc simple
	.cfi_def_cfa esp, 0
	CFI_UTF_OFFSETS
	call	*%ecx
	.cfi_endproc

ENTRY(utrap_ret)
	// UTrapframe pointer passed in via %eax

	// Mask traps first, to ensure exclusive use of tls_utrap_ret_buf
	pushl	$GD_UT_MASK
	pushl	$utrap_ret_masked
	lret

utrap_ret_masked:
	// Restore eflags -- be careful not to modify them afterwards!
	pushl	utf_eflags(%eax)
	popf

	// Construct a ljmp target buffer..
	movl	$0, %ecx
	movl	$4, %edx

	movl	utf_eip(%eax), %ebx
	movl	%ebx, tls_utrap_ret_buf(%ecx)
	movl	$GD_UT_NMASK, tls_utrap_ret_buf(%edx)

	// Restore all registers except eflags, eax, and eip.
	movl	utf_ebx(%eax), %ebx
	movl	utf_ecx(%eax), %ecx
	movl	utf_edx(%eax), %edx

	movl	utf_esi(%eax), %esi
	movl	utf_edi(%eax), %edi
	movl	utf_ebp(%eax), %ebp
	movl	utf_esp(%eax), %esp

	// Restore eax, overwriting the UTrapframe address
	movl	utf_eax(%eax), %eax

	// Restore eip and unmask traps
	ljmp	*tls_utrap_ret_buf

ENTRY(utrap_set_cs)
	popl	%ecx		// return %eip
	xorl	%eax, %eax
	movw	0(%esp), %ax	// new %cs
	pushl	%eax
	pushl	%ecx		// return %eip
	lret

