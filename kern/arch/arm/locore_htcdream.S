#include <machine/arm.h>
#include <machine/asm.h>
#include <machine/mmu.h>
#include <machine/memlayout.h>

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

	// temporarily map first 640MB of KVA into PA at address 0
	// we do this because we need to be able to continue executing
	// after switching the MMU on!
	mov	r7, #0			// r7 <- 0 (1MB section number)
	add	r8, r7, #640		// from [0, 640)...
1:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [0, 640))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	1b			// ...and loop if not

	// map the 384MB PA range 256MB-640MB to KVA at 2GB
	mov	r7, #2048
	add	r8, r7, #384		// from [2048, 2432)... 
2:
	mov	r9, #ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	orr	r9, r9, #ARM_MMU_L1_SECTION_BUFFERABLE
	orr	r9, r9, #ARM_MMU_L1_SECTION_CACHEABLE
	sub	r10, r7, #1792		// r10 = i	(i in [256, 640))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10]		// set kpagemap entry
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	2b			// ...and loop if not

	// map last 1.625GB of PA to KVA at 2.375GB
	// r7 == 2432
	add	r8, r7, #1664		// from [2432, 4096)...
3:
	mov	r9, POUND ARM_MMU_L1_TYPE_SECTION
	orr	r9, r9, #ARM_MMU_L1_SECTION_AP(ARM_MMU_AP_KRW)
	mov	r10, r7			// r10 = i	(i in [2432, 4096))
	lsl	r10, r10, #20		// r10 <<= 20 (convert to 1MB offset)
	orr	r9, r9, r10		// r9 |= r10
	lsl	r10, r7, #2		// get relative byte offset
	add	r10, r10, r6		// get absolute offset
	str	r9, [r10], #4		// set kpagemap[i++] = r9
	add	r7, r7, #1		// increment 1MB section number
	cmp	r7, r8			// check if done...
	blt	3b			// ...and loop if not

	// jump (fall, really, but just in case...) to common init code
	b start_common
