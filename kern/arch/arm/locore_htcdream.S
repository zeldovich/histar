#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/mmu.h>
#include <machine/memlayout.h>

#define INIT_STACK_SIZE        2048

// # stringifies in cpp, but used for constants in arm asm.
#define POUND #

/*
 * start:
 *
 *    First instruction executed. Set up our kernel page table, enable the
 *    mmu, and bootstrap just enough to call into init.
 */
.text
.global start
start:
	// load the address of kpagemap, the kernel level 1 page map struct
	adr	r6, kpagemap_reloc_addr	// load page table physical address
	ldr	r6, [r6]

	// temporarily map first 768MB of KVA into PA at address 0
	// we do this because we need to be able to continue executing
	// after switching the MMU on!
	mov	r7, #0			// r7 <- 0 (1MB section number)
	add	r8, r7, #768		// from [0, 768)...
1:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [0, 768))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	1b			// ...and loop if not

	// map the 512MB PA range 256MB-768MB to KVA at 2GB
	mov	r7, #2048
	add	r8, r7, #512		// from [2048, 2560)... 
2:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	sub	r10, r7, #1792		// r10 = i	(i in [256, 768))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	2b			// ...and loop if not

	// map last 1.5GB of PA to KVA at 2.5GB
	// r7 == 2560
	add	r8, r7, #1536		// from [2560, 4096)...
3:
	mov	r9, POUND ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [2560, 4096))
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

uart3_tfrf:
	.word 0xa9c0000c

init_addr:
	.word init

kpagemap_reloc_addr:
	.word RELOC(kpagemap)

init_stack_top_addr:
        .word   init_stack_addr + INIT_STACK_SIZE

.align  8				// ARM EABI requires 8-byte sp alignment
init_stack_addr:
        .space  INIT_STACK_SIZE
