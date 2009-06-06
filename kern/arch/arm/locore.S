#if defined(JOS_ARM_GOLDFISH)
#include "locore_goldfish.S"
#elif defined(JOS_ARM_HTCDREAM)
#include "locore_htcdream.S"
#else
#error unknown arm target
#endif

#define INIT_STACK_SIZE        2048

/*
 * Jumped to from the board-specific `start' routine. It's assumed that the
 * kpagemap is set up and we're ready to enable the MMU and jump to init(). 
 */
start_common:
	// load the address of kpagemap, the kernel level 1 page map struct
	adr	r6, kpagemap_reloc_addr	// load page table physical address
	ldr	r6, [r6]

	// set up the mmu and enable translation. we must:
	//    1) disable and invalidate the icache
	mov	r7, #0
	mcr	p15, 0, r7, CP15_CTRL_REG, c0, 0
	mcr	p15, 0, r7, CP15_CWB_CTRL_REG, c5, 0

	//    2) set the domain access values in register 3 
	mov	r7, #ARM_MMU_DOMAINx(0, ARM_MMU_DOMAIN_CLIENT)
	mcr	p15, 0, r7, CP15_DOMACC_CTRL_REG, c0, 0

	//    3) load our page table pointer in TTBR (register 2)
	mcr	p15, 0, r6, CP15_PGTBL_CTRL_REG, c0, 0

	//    4) flush the TLB (paranoia)
	mov	r7, #0
	mcr	p15, 0, r7, CP15_TLB_CTRL_REG, c7, 0

	//    5) enable the MMU
	mov	r7, #CTRL_M
	mcr	p15, 0, r7, CP15_CTRL_REG, c0, 0
	
	// set up our stack and jump to init
	adr     sp, init_stack_top_addr	// set our temporary kernel stack.
	ldr	sp, [sp]
	mov	fp, #0x00000000		// necessary?!
	adr	r10, init_addr		// load init's full addr (not rel. addr)
	ldr	pc, [r10]		// jump to init(), using kva

1:	b	1b			// We're in trouble...

init_addr:
	.word init

kpagemap_reloc_addr:
	.word RELOC(kpagemap)

init_stack_top_addr:
        .word   init_stack_addr + INIT_STACK_SIZE

.align  8				// ARM EABI requires 8-byte sp alignment
init_stack_addr:
        .space  INIT_STACK_SIZE

/*****************************
 * cache/tlb/misc routines   *
 *****************************/

ENTRY(cpsr_get)
	mrs	r0, cpsr
	mov	pc, lr

ENTRY(cpsr_set)
	msr	cpsr, r0
	mov	pc, lr

ENTRY(cp15_ctrl_get)
	mrc	p15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cp15_ctrl_set)
	mcr	p15, 0, r0, c1, c0, 0
	mov	pc, lr

ENTRY(cp15_cop_acc_ctrl_get)
	mrc	p15, 0, r0, c1, c0, 2
	mov	pc, lr

ENTRY(cp15_cop_acc_ctrl_set)
	mcr	p15, 0, r0, c1, c0, 2
	mov	pc, lr

ENTRY(cp15_fsr_get)
	mrc	p15, 0, r0, c5, c0, 0
	mov	pc, lr

ENTRY(cp15_fsr_set)
	mcr	p15, 0, r0, c5, c0, 0
	mov	pc, lr

ENTRY(cp15_far_get)
	mrc	p15, 0, r0, c6, c0, 0
	mov	pc, lr

ENTRY(cp15_far_set)
	mcr	p15, 0, r0, c6, c0, 0
	mov	pc, lr

ENTRY(cp15_ttbr_get)
	mrc	p15, 0, r0, c2, c0, 0
	mov	pc, lr

ENTRY(cp15_ttbr_set)
	mcr	p15, 0, r0, c2, c0, 0
	mov	pc, lr

ENTRY(cp15_tlb_flush)
	mcr	p15, 0, r0, c8, c7, 0
	mov	pc, lr

ENTRY(cp15_tlb_flush_entry_arm11)
	mcr     p15, 0, r0, c8, c6, 1		// flush entry at va == r0
        mcr     p15, 0, r0, c7, c10, 4		// data synch barrier
	mov	pc, lr

ENTRY(cp15_write_buffer_drain)
	mcr	p15, 0, r0, c7, c10, 4		// data synch barrier
	mov	pc, lr

ENTRY(cp15_icache_invalidate)
	mcr	p15, 0, r0, c7, c5, 0
	mov	pc, lr

ENTRY(cp15_icache_invalidate_arm11)
	// Erratum 411920 workaround
	mov     r0, #0				// should be zero (SBZ)
	mrs     r1, cpsr
#ifdef JOS_ARM_HTCDREAM				// XXX arm11-specific
	cpsid   ifa
#endif
	mcr     p15, 0, r0, c7, c5, 0
	mcr     p15, 0, r0, c7, c5, 0
	mcr     p15, 0, r0, c7, c5, 0
	mcr     p15, 0, r0, c7, c5, 0
	msr     cpsr_cx, r1
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	mov	pc, lr

ENTRY(cp15_dcache_flush_invalidate_range_arm11)
	sub	r1, r1, #1
	add	r1, r0, r1
	mcrr	p15, 0, r1, r0, c14		// flush+inval cache range
	mcr	p15, 0, r0, c7, c10, 4		// data synch barrier
	mov	pc, lr

ENTRY(cp15_dcache_flush_invalidate_arm11)
	// flush + inval entire cache, followed by data synch barrier
	mov	r0, #0
	mcr	p15, 0, r0, c7, c14, 0		// flush+inval entire cache
	mcr	p15, 0, r0, c7, c10, 4		// data synch barrier
	mov	pc, lr

ENTRY(cp15_dcache_flush_invalidate_tci)
	// loop over dirty lines and flush + inval
1:	mcr	p15, 0, r0, c7, c14, 3
	bne	1b
	mov	pc, lr

ENTRY(cp15_dcache_flush_invalidate)
	//XXX- iterate over sets/ways and issue flushes
	mov	pc, lr

ENTRY(cp15_main_id_get)
	mrc	p15, 0, r0, c0, c0, 0
	mov	pc, lr

ENTRY(cp15_cache_type_get)
	mrc	p15, 0, r0, c0, c0, 1
	mov	pc, lr
