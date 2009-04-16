#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/mmu.h>
#include <machine/memlayout.h>
#include <machine/trapcodes.h>

// # stringifies in cpp, but used for constants in arm asm.
#define POUND #

/*
 * exception_vector:
 *
 *    The exception vector contains instructions executed upon various events.
 *    It is to be copied to KVA 0xffff0000 (high mem; V bit in CP15). Note
 *    that we use a non-vectored IRQ and FIQ (need VE in CP15 == 0).
 *
 */
.global exception_vector
exception_vector:
	ldr	pc, [pc, #24]		// pc 8 bytes ahead, load from here+32
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]
	ldr	pc, [pc, #24]

	.word reset_vector
	.word undefined_instruction_vector
	.word swi_vector
	.word prefetch_abort_vector
	.word data_abort_vector
	.word unused_vector
	.word irq_vector
	.word fiq_vector


/*
 * The following hold exception stack addresses. We're not doing nested
 * exceptions right now.
 */
.global exception_vector_stack_addrs
exception_vector_stack_addrs:
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef
	.word 0xdeadbeef


/*
 * Finally, the exception vectors themselves.
 *
 * Note (from ARM ARM):
 *
 *   "To return after handling the exception, the SPSR is moved into the
 *    CPSR, and R14 is moved to the PC. This can be done atomically in two
 *    ways:
 *       - using a data-processing instruction with the S bit set, and the
 *         PC as the destination
 *       - using the Load Multiple with Restore CPSR instruction, as described 
 *         in LDM (3) on page A4-40."
 */
#define EXCEPTION_HANDLE(_typ, _spo, _lrfix)				      \
	adr	sp, exception_vector_stack_addrs+_spo;	/* load stack */      \
	ldr	sp, [sp];				/*   ... */	      \
	add	lr, lr, POUND _lrfix;			/* pc return fixup */ \
	str	lr, [sp, #-4]!;				/* push fixed up lr*/ \
	stmdb	sp, {r0-r14}^;				/* push "user" regs*/ \
	mov	r0, r0;					/* nop */	      \
	sub	sp, sp, #60;				/* adjust stack */    \
	mov	r0, POUND _typ;				/* exception type */  \
	mrs	r1, spsr;				/* get spsr */        \
	str	r1, [sp, #-4]!;				/*   push it */       \
	mov	r1, sp;					/* trampframe ptr */  \
	sub	sp, sp, #4;				/* 8-byte align sp */ \
	mov	r2, sp;					/* assertion */	      \
	b	exception_handler;					      \
									      \
	ldr	r0, file_str_addr;			/* shouldn't happen */\
	mov	r1, POUND __LINE__;					      \
	ldr	r2, impossible_panic_str_addr;				      \
	b	_panic

#define EXCEPTION_PANIC(_spo)						      \
	adr	sp, exception_vector_stack_addrs+_spo;	/* load stack */      \
	ldr	sp, [sp];				/*   ... */	      \
	ldr	r0, file_str_addr;			/* _panic file str */ \
	mov	r1, POUND __LINE__;			/* _panic line */     \
	ldr	r2, exception_panic_str_addr;		/* _panic str */      \
	b	_panic


// Pop the trapframe pointed to by r0.
.global trapframe_pop
trapframe_pop:
	mov	sp, r0					// use banked sp as tmp
	ldr	r0, [sp], #4				// pop spsr (to be cpsr)
	msr	spsr_all, r0				// restore spsr
	ldmia	sp, {r0-r14}^				// pop "user" regs
	mov	r0, r0					// nop
	add	sp, sp, #60				// adjust sp to &tf.r15
	ldr	lr, [sp], #4				// pop the pc
	movs	pc, lr					// restore cpsr & pc

	ldr	r0, file_str_addr			// shouldn't happen
	mov	r1, #__LINE__
	ldr	r2, impossible_panic_str_addr
	b	_panic

reset_vector:
	EXCEPTION_PANIC(0)


undefined_instruction_vector:
	EXCEPTION_HANDLE(T_UI, 4, 0)


swi_vector:
	EXCEPTION_HANDLE(T_SWI, 8, 0)


prefetch_abort_vector:
	EXCEPTION_HANDLE(T_PA, 12, -4)


data_abort_vector:
	EXCEPTION_HANDLE(T_DA, 16, -8)


unused_vector:
	EXCEPTION_PANIC(20)


irq_vector:
	EXCEPTION_HANDLE(T_IRQ, 24, -4)


fiq_vector:
	EXCEPTION_PANIC(28)

file_str_addr:
	.word file_str
exception_panic_str_addr:
	.word exception_panic_str
impossible_panic_str_addr:
	.word impossible_panic_str

file_str:
	.asciz	__FILE__
.align 4
impossible_panic_str:
	.asciz "IMPOSSIBLE EXECUTION"
.align 4
exception_panic_str:
	.asciz "UNHANDLED EXCEPTION"
