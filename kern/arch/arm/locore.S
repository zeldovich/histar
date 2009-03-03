#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/mmu.h>
#include <machine/memlayout.h>
#include <machine/trapcodes.h>

#define INIT_STACK_SIZE        2048

// # stringifies in cpp, but used for constants in arm asm.
#define POUND #


/*
 * start:
 *
 *    First instruction executed. Set up our kernel page table, enable the
 *    mmu, and bootstrap just enough to call init_phys(), then switch our
 *    stack and jump into init_virt.
 */
.text
.global start
start:
	// load the address of kpagemap, the kernel level 1 page map struct
	adr	r6, kpagemap_reloc_addr	// load page table physical address
	ldr	r6, [r6]

	// temporarily map the first 2GB of KVA into PA at address 0
	// we do this because we need to be able to continue executing
	// after switching the MMU on!
	mov	r7, #0			// r7 <- 0 (1MB section number)
	add	r8, r7, #2048		// from [0, 2048)... 
1:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [0, 2048))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	1b			// ...and loop if not

	// map first 1GB of PA to KVA at 2GB
	// r7 == 2048
	add	r8, r7, #1024		// from [2048, 3072)... 
2:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	sub	r10, r7, #2048		// r10 = i	(i in [0, 1024))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	2b			// ...and loop if not

	// map last 1GB of PA to KVA at 3GB
	// r7 == 3072*4
	add	r8, r7, #1024		// from [3072, 4096)...
3:
	mov	r9, POUND ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [3072, 4096))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10], #4		// set kpagema[i++] = r9
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	3b			// ...and loop if not

	// set up the mmu and enable translation. we must:
	//    1) disable and invalidate the icache
	//    2) set the domain access values in register 3 
	//    3) load our page table pointer in TTBR (register 2)
	//    4) flush the TLB (paranoia)
	//    5) enable the MMU
					// 1. XXX disable and inval icache
	mov	r7, #ARM_MMU_DOMAINx(0, ARM_MMU_DOMAIN_CLIENT)
	mcr	p15, 0, r7, c3, c0, 0	// 2. set domain access control
	mcr	p15, 0, r6, c2, c0, 0	// 3. load &kpagemap into the TTBR
	mov	r7, #0
	mcr	p15, 0, r7, c8, c7, 0	// 4. flush the tlb(s)
	mov	r7, #CTRL_M
	mcr	p15, 0, r7, c1, c0, 0	// 5. enable the MMU
	
	// set up our stack and jump to init
	adr     sp, init_stack_top_addr	// set our temporary kernel stack.
	ldr	sp, [sp]
	mov	fp, #0x00000000		// necessary?!
	adr	r10, init_addr		// load init's full addr (not rel. addr)
	ldr	pc, [r10]		// jump to init(), using kva
4:
	b	4b			// We're in trouble...

init_addr:
	.word init

kpagemap_reloc_addr:
	.word RELOC(kpagemap)

init_stack_top_addr:
        .word   init_stack_addr + INIT_STACK_SIZE

.align  8				// ARM EABI requires 8-byte sp alignment
init_stack_addr:
        .space  INIT_STACK_SIZE


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
