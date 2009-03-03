#include <machine/asm.h>
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

// XXX- THIS ISN'T ATOMIC!!! HOW TO FIX THIS? SEE i386... -XXX
ENTRY(utrap_ret)
	mov	r5, r0			// preserve ptr and call utrap_set_mask
	mov	r0, #0 //UT_NMASK 
	bl	utrap_set_mask

	add	r5, r5, #4		// [0] = spsr, [1,...,16] = r0,...,r16
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
