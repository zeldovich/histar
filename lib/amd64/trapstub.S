#include <machine/asm.h>

ENTRY(utrap_stub)
	// This piece has to be position-independent.
	movq	$utrap_entry_asm, %rax
	jmp	*%rax
ENTRY(utrap_stub_end)
	nop

ENTRY(utrap_entry_asm)
	.cfi_startproc

	// To be sure, this is what CFA always starts as, on function entry.
	.cfi_def_cfa rsp, 8

	// First of all, save the address of the UTrapframe
	movq	%rsp, %rdi

	// Now make up a fake call-frame from the faulting instruction
	pushq	utf_rip(%rdi)

	// Tell DWARF2 about our saved registers
	.cfi_offset rax,  0*8
	.cfi_offset rbx,  1*8
	.cfi_offset rcx,  2*8
	.cfi_offset rdx,  3*8

	.cfi_offset rsi,  4*8
	.cfi_offset rdi,  5*8
	.cfi_offset rbp,  6*8
	.cfi_offset rsp,  7*8

	.cfi_offset r8,   8*8
	.cfi_offset r9,   9*8
	.cfi_offset r10, 10*8
	.cfi_offset r11, 11*8

	.cfi_offset r12, 12*8
	.cfi_offset r13, 13*8
	.cfi_offset r14, 14*8
	.cfi_offset r15, 15*8

	// Off we go
	call	utrap_entry
	.cfi_endproc

ENTRY(utrap_ret)
	movq	%rdi, %rsp

	// Generate an iret frame on the stack to atomically restore
	// rflags, rsp and rip.
	//
	// Even though we are writing below our stack pointer, it's OK;
	// it's in our red zone (128 bytes below %rsp) -- see ABI spec.
	movq	$0, -8(%rsp)
	movw	%ss, -8(%rsp)

	movq	utf_rsp(%rsp), %rax
	movq	%rax, -16(%rsp)

	movq	utf_rflags(%rsp), %rax
	movq	%rax, -24(%rsp)

	movq	$0, -32(%rsp)
	movw	%cs, -32(%rsp)

	movq	utf_rip(%rsp), %rax
	movq	%rax, -40(%rsp)

	// Restore all registers except rflags, rsp and rip.
	movq	utf_rax(%rsp), %rax
	movq	utf_rbx(%rsp), %rbx
	movq	utf_rcx(%rsp), %rcx
	movq	utf_rdx(%rsp), %rdx

	movq	utf_rsi(%rsp), %rsi
	movq	utf_rdi(%rsp), %rdi
	movq	utf_rbp(%rsp), %rbp

	movq	utf_r8 (%rsp), %r8
	movq	utf_r9 (%rsp), %r9
	movq	utf_r10(%rsp), %r10
	movq	utf_r11(%rsp), %r11

	movq	utf_r12(%rsp), %r12
	movq	utf_r13(%rsp), %r13
	movq	utf_r14(%rsp), %r14
	movq	utf_r15(%rsp), %r15

	// Restore rip, rsp and rflags using the iret frame.
	subq	$40, %rsp
	iretq

