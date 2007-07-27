#include <machine/asm.h>
#include <machine/pmap.h>
#include <obj/lib/utrapasm.h>

/*
 * When we upgrade to binutils 2.17, we should start using .cfi_signal_frame:
 * http://gcc.gnu.org/bugzilla/show_bug.cgi?id=26208
 */

#define CFI_UTF_OFFSETS		\
	.cfi_offset rax,  0*8;	\
	.cfi_offset rbx,  1*8;	\
	.cfi_offset rcx,  2*8;	\
	.cfi_offset rdx,  3*8;	\
				\
	.cfi_offset rsi,  4*8;	\
	.cfi_offset rdi,  5*8;	\
	.cfi_offset rbp,  6*8;	\
	.cfi_offset rsp,  7*8;	\
				\
	.cfi_offset r8,   8*8;	\
	.cfi_offset r9,   9*8;	\
	.cfi_offset r10, 10*8;	\
	.cfi_offset r11, 11*8;	\
				\
	.cfi_offset r12, 12*8;	\
	.cfi_offset r13, 13*8;	\
	.cfi_offset r14, 14*8;	\
	.cfi_offset r15, 15*8;	\
				\
	.cfi_offset rip, 16*8;

ENTRY(utrap_entry_asm)
	.cfi_startproc simple

	// Our entire CPU state is saved in a UTrapframe right at %rsp
	.cfi_def_cfa rsp, 0

	// Tell DWARF2 about our saved registers; this is UTrapframe layout
	CFI_UTF_OFFSETS

	// Pass the address of the UTrapframe to utrap_entry()
	movq	%rsp, %rdi

	// Call instead of jmp, because we want the call stack to point back to
	// this function, so that the unwinder sees our register remapping.
	call	utrap_entry
	.cfi_endproc

ENTRY(utrap_chain_dwarf2)
	.cfi_startproc simple
	.cfi_def_cfa r14, 0
	CFI_UTF_OFFSETS
	call	*%r15
	.cfi_endproc

ENTRY(utrap_ret)
	// %rdi contains our UTrapframe address

	// Generate an iret frame on the stack to atomically restore
	// rflags, rsp and rip.
	subq	$40, %rsp
	movq	$0, 32(%rsp)
	movw	%ss, 32(%rsp)

	movq	utf_rsp(%rdi), %rax
	movq	%rax, 24(%rsp)

	movq	utf_rflags(%rdi), %rax
	movq	%rax, 16(%rsp)

	movq	$0, 8(%rsp)
	movw	$GD_UT_NMASK, 8(%rsp)		// %cs

	movq	utf_rip(%rdi), %rax
	movq	%rax, 0(%rsp)

	// Restore all registers except rflags, rsp and rip.
	movq	utf_rax(%rdi), %rax
	movq	utf_rbx(%rdi), %rbx
	movq	utf_rcx(%rdi), %rcx
	movq	utf_rdx(%rdi), %rdx

	movq	utf_rsi(%rdi), %rsi
	movq	utf_rbp(%rdi), %rbp

	movq	utf_r8 (%rdi), %r8
	movq	utf_r9 (%rdi), %r9
	movq	utf_r10(%rdi), %r10
	movq	utf_r11(%rdi), %r11

	movq	utf_r12(%rdi), %r12
	movq	utf_r13(%rdi), %r13
	movq	utf_r14(%rdi), %r14
	movq	utf_r15(%rdi), %r15

	// Restore rdi last, overwriting the UTrapframe pointer
	movq	utf_rdi(%rdi), %rdi

	// Restore rip, rsp and rflags using the iret frame.
	iretq

ENTRY(utrap_set_cs)
	popq	%rcx		// return %eip
	pushq	%rdi		// new %cs
	pushq	%rcx		// return %eip
	lretq

