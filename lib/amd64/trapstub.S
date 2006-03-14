#include <machine/asm.h>

ENTRY(utrap_stub)
	movq	$utrap_entry, %rax
	movq	%rsp, %rdi
	jmp	*%rax
ENTRY(utrap_stub_end)

ENTRY(utrap_ret)
	// Shift the return stack down by 8
	movq	utf_rsp(%rdi), %rax
	subq	$8, %rax
	movq	%rax, utf_rsp(%rdi)

	// Store the return RIP in that 8 bytes
	movq	utf_rip(%rdi), %rbx
	movq	%rbx, (%rax)

	// Restore all registers except RSP & RIP
	movq	%rdi, %rsp

	push	utf_rflags(%rsp)
	popfq

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

	// Now restore RSP and return (the RIP is already there)
	movq	utf_rsp(%rsp), %rsp
	ret

