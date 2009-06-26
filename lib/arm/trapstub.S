#include <machine/asm.h>
#include <machine/arm.h>
#include <machine/pmap.h>
#include <machine/mmu.h>
#include <lib/utrapasm.h>

ENTRY(utrap_entry_asm)
	mov	r0, sp			// utf on stack at sp
	bl	utrap_entry

	ldr	r0, file_str_addr	// shouldn't happen
	mov	r1, #__LINE__
	ldr	r2, utrap_entry_asm_panic_str_addr
	b	_panic

#ifdef __thumb__
ENTRY(utrap_set_mask_tramp)
	.code 16
	.thumb_func
	push	{lr}
	bl	utrap_set_mask
	pop	{r0}
	bx	r0
	.code 32
#endif

// XXX- THIS ISN'T ATOMIC!!! HOW TO FIX THIS? SEE i386... -XXX
ENTRY(utrap_ret)
	mov	r5, r0			// preserve ptr and call utrap_set_mask
	add	r5, r5, #4		// [0] = spsr, [1,...,16] = r0,...,r15

#ifdef __thumb__
	mov	r0, #0 //UT_NMASK 
	str	r5, [sp, #-4]!		// save utrap frame ptr
	adr	r1, utrap_set_mask_tramp
	orr	r1, r1, #0x1		// ensure bx will switch modes
	blx	r1
	ldr	r5, [sp], #4		// restore utrap frame ptr

	ldr	r0, [r5, #-4]		// grab spsr
	and	r0, r0, #CPSR_ISET_THUMB // check if we were thumb or arm
	cmp	r0, #CPSR_ISET_THUMB
	bne	ret_arm

ret_thumb:
	ldr	r9, [r5, #60]		// grab pc
	orr	r9, r9, #0x1		// pc |= 1 (=> thumb mode)
	ldr	sp, [r5, #52]		// switch stacks
	str	r9, [sp, #-4]		// save pc to stack, but don't sub yet
	ldmia	r5, {r0-r14}		// restore most regs
	str	r0, [sp, #-8]		// save r0 to stack
	sub	sp, sp, #8		// adjust for pc, r0
	adr	r0, ret_thumb_tramp	// get thumb trampoline address 
	orr	r0, r0, #0x1		// ensure bx will switch modes

	// at this point, we need only switch into thumb mode and pop r0, pc
	bx	r0

ret_thumb_tramp:
	.code 16
	.thumb_func
	pop	{r0,pc}			// restore r0 and jump to pc
	.code 32
#endif

ret_arm:
#ifndef __thumb__
	mov	r0, #0 //UT_NMASK 
	str	r5, [sp, #-4]!		// save utrap frame ptr
	bl	utrap_set_mask
	ldr	r5, [sp], #4		// restore utrap frame ptr
#endif

	ldmia	r5, {r0-r15}		// I <3 ARM

	ldr	r0, file_str_addr	// shouldn't happen
	mov	r1, #__LINE__
	ldr	r2, utrap_ret_panic_str_addr
	b	_panic

file_str_addr:
	.word file_str
utrap_entry_asm_panic_str_addr:
	.word utrap_entry_asm_panic_str
utrap_ret_panic_str_addr:
	.word utrap_ret_panic_str

file_str:
	.asciz __FILE__
.align 4
utrap_entry_asm_panic_str:
	.asciz "utrap_entry_asm: utrap_entry returned"
.align 4
utrap_ret_panic_str:
	.asciz "utrap_ret: impossible execution"
