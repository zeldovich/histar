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
	orr	r9, r9, #ARM_MMU_L1_SECTION_BUFFERABLE
	orr	r9, r9, #ARM_MMU_L1_SECTION_CACHEABLE
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
	// r7 == 3072
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

	// jump (fall, really, but just in case...) to common init code
	b start_common
