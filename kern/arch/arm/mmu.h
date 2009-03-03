#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#define	PGSHIFT		12
#define	PGSIZE		(1 << PGSHIFT)
#define PGMASK		(PGSIZE - 1)
#define PGOFF(la)	(((uintptr_t) (la)) & PGMASK)

#define PTE_ADDR(e)	((e) & ~(1023))	/* L2 entires 1k in size, 1k aligned */
#define NL1PTENTRIES    4096		/* 1MB/entry */
#define NL2PTENTRIES	256		/* 4KB/entry (for 'small' pgs) */
#define NPTLVLS		1		/* page tbl depth - 1 (=>2-level TLB) */
#define PTE_SIZE	4		/* 32-bit entries */
#define L1_PT_SIZE	(NL1PTENTRIES * PTE_SIZE)
#define L2_PT_SIZE	(NL2PTENTRIES * PTE_SIZE)

#define L1_PT_SHIFT	20
#define L2_PT_SHIFT	12

#define ADDR_2_L1IDX(x)	((x) >> L1_PT_SHIFT)		/* L1 maps 4096 1MBpgs*/
#define ADDR_2_L2IDX(x) (((x) >> L2_PT_SHIFT) & 0xff)	/* L2 maps 256 4KB pgs*/

/*
 * ARM First-level Descriptor MMU bits
 *
 * ARM has a 2 level page table with coarse and section pages. Sections may
 * refer directly to 1MB. Coarse entries refer to a second level page table,
 * which has various page sizes from 64KB to 1KB. We'll use 4KB.
 *
 * VMSAv6 also defines 16MB `supersections', but we'll ignore them for better
 * compatibility.
 *
 * ARM also has a notion of `domains'. Level 1 page table entries can be
 * assigned to one of 16 domains, which have various repercussions for TLB
 * access permission bit checking. We won't use this feature, but we _must_
 * set up the permissions appropriately for domain 0. Permissions for each
 * of the 16 domains is represented by 2 bits in CP15 register 3.
 *
 * NOTE: The L1 page table has 4096 entries (16KB total) and _must_ be 16KB
 *       aligned! The TTBR register completely ignores bits 0-13.
 */

/*
 * Page/subpage/section `access permission' bits (AP), used in level 1 and
 * level 2 entries below. 
 */
#define ARM_MMU_AP_NOACCESS		0x0		/* always invalid */
#define ARM_MMU_AP_KRW			0x1		/* kern r/w, no user */ 
#define ARM_MMU_AP_KRWURO		0x2		/* kern r/w, user r/o*/ 
#define ARM_MMU_AP_KRWURW		0x3		/* kern r/w, user r/w*/ 
#define ARM_MMU_AP_MASK			0x3

/*
 * First level page table definitions:
 */
#define ARM_MMU_L1_ALIGNMENT		(16 * 1024)

#define ARM_MMU_L1_TYPE_MASK		0x3
#define ARM_MMU_L1_TYPE(x)		((x) & ARM_MMU_L1_TYPE_MASK)
#define ARM_MMU_L1_TYPE_INVALID		0x0
#define ARM_MMU_L1_TYPE_COARSE		0x1		/* ptr to level 2 pte */
#define ARM_MMU_L1_TYPE_SECTION		0x2		/* maps 1MB directly */

#define ARM_MMU_L1_COARSE_MASK		0xfffffc00	/* bits for lvl 2 pte */
#define ARM_MMU_L1_SECTION_MASK		0xfff00000	/* bits for direct map*/

#define ARM_MMU_L1_SECTION_AP(x)	((x) << 10)

#define ARM_MMU_DOMAINx(x, val)		((val) << ((x)*2))
#define ARM_MMU_DOMAIN_NOACCESS		0x00		/* any access faults */
#define ARM_MMU_DOMAIN_CLIENT		0x01		/* accesses use TLB AP*/
#define ARM_MMU_DOMAIN_RESERVED		0x02		/* reserved */
#define ARM_MMU_DOMAIN_MANAGER		0x03		/* don't check TLB AP */

/*
 * ARM Second-level Descriptor MMU bits 
 *
 * Level 2 descriptors have 256 entries and are 1KB is size. We will use
 * `small pages', i.e. 4KB. Each large and small page entry can set individual
 * permissions of subpages of the page. In our case, those bits will be the
 * same for each subpage.
 */

/*
 * Second level page definitions:
 */
#define ARM_MMU_L2_ALIGNMENT		(1024)

#define ARM_MMU_L2_TYPE_MASK		0x3
#define ARM_MMU_L2_TYPE(x)		((x) & ARM_MMU_L2_TYPE_MASK)
#define ARM_MMU_L2_TYPE_INVALID		0x0
#define ARM_MMU_L2_TYPE_LARGE		0x1		/* 64KB page */
#define ARM_MMU_L2_TYPE_SMALL 		0x2		/* 4KB page */
#define ARM_MMU_L2_TYPE_EXTENDED	0x3		/* no bloody clue */

#define ARM_MMU_L2_CACHEABLE		0x08
#define ARM_MMU_L2_BUFFERABLE		0x04

#define ARM_MMU_L2_SMALL_AP(x)	(((x)<<4) | ((x)<<6) | ((x)<<8) | ((x)<<10))
#define ARM_MMU_L2_SMALL_AP_MASK	ARM_MMU_L2_SMALL_AP(ARM_MMU_AP_MASK)

#define ARM_MMU_L2_LARGE_MASK		0xffff0000	/* bits for pg base */
#define ARM_MMU_L2_SMALL_MASK		0xfffff000	/* bits for pg base */
#define ARM_MMU_L2_EXTENDED_MASK	0xfffff000	/* bits for pg base */

/*
 * Fault Status Register (FSR) and Fault Address Register (FAR) bits.
 * FSR informs us of a data or prefetch fault and FAR contains the faulting
 * address.
 *
 * Somewhat annoyingly, prior to VMSAv6 the FSR does not indicate whether an
 * abort occured during a page write or not.
 *
 * XXX- FAR might contain a MVA if FCSE is in use?!
 */
#define ARM_MMU_FSR_DOMAIN_MASK	0x000000f0		/* responsible domain */
#define ARM_MMU_FSR_DOMAIN_SHFT	4
#define ARM_MMU_FSR_DOMAIN(x)	(((x) & ARM_MMU_FSR_DOMAIN_MASK) >> \
					ARM_MMU_FSR_DOMAIN_SHFT)

#define ARM_MMU_FSR_FS_MASK	0x0000040f		/* fault status bits */

/*
 * The following are defined for all ARM32. FAR is valid for all of them.
 * FSR's domain field is valid in all cases _except_ for:
 *     ARM_MMU_FSR_FS_EATL1 and ARM_MMU_FSR_FS_TRANS_S.
 */
#define ARM_MMU_FSR_FS_EATL1	0x00c		/* extern. abt on l1 xlation */
#define ARM_MMU_FSR_FS_EATL2	0x00e		/* extern. abt on l2 xlation */
#define ARM_MMU_FSR_FS_TRANS_S	0x005		/* xlation abt on section */
#define ARM_MMU_FSR_FS_TRANS_P	0x007		/* xlation abt on page */
#define ARM_MMU_FSR_FS_DOMAIN_S	0x009		/* domain abt on section */
#define ARM_MMU_FSR_FS_DOMAIN_P	0x00b		/* domain abt on page */
#define ARM_MMU_FSR_FS_PERM_S	0x00d		/* permission abt on section */
#define ARM_MMU_FSR_FS_PERM_P	0x00f		/* permission abt on page */

/*
 * The following are for VSMAv6 only. The domain field is invalid for all
 * except for ARM_MMU_FSR_FS_DEBUG.
 *     (d) => deprecated, (i) => impl. defined, (f) => FAR invalid/undefined
 */
#define ARM_MMU_FSR_FS_TLBMISS	0x000		/* PMSA - TLB miss */
#define ARM_MMU_FSR_FS_ALIGN	0x001		/* alignment fault */
#define ARM_MMU_FSR_FS_OALIGN	0x003		/* (d) alignment fault */
#define ARM_MMU_FSR_FS_ICMOF	0x008		/* icache mgmt. op fault */
#define ARM_MMU_FSR_FS_PEA	0x008		/* precise extern. abt */
#define ARM_MMU_FSR_FS_EAP	0x00a		/* (d) extern. abt, precise */
#define ARM_MMU_FSR_FS_TLBLK	0x404		/* (f) tlb lock abt */
#define ARM_MMU_FSR_FS_CDA	0x40a		/* (i,f) coproc data abt */
#define ARM_MMU_FSR_FS_IEA	0x406		/* imprecise extern.  abt */
#define ARM_MMU_FSR_FS_PARITY	0x408		/* (f) parity error */
#define ARM_MMU_FSR_FS_DEBUG	0x002		/* (f) debug event */

#define UINT32(_x)	((uint32_t)(_x))

#ifndef __ASSEMBLER__
#include <inc/thread.h>

/*
 * Quick note about regs:
 *   r0-r7   Always unbanked (i.e. the same). Referred to as "user registers".
 *   r8-r12  Banked in FIQ mode only, otherwise the same.
 *   r13-r14 Banked in all modes (sp and lr).
 *
 * Include the full register set for completeness. Several of these
 * are banked, so they don't always have to be saved. However, sometimes
 * we'll have to (e.g. context switch to new process) and it doesn't hurt.
 */
struct Trapframe {
	uint32_t	tf_spsr;	// saved cpsr
	uint32_t	tf_r0;
	uint32_t	tf_r1;
	uint32_t	tf_r2;
	uint32_t	tf_r3;
	uint32_t	tf_r4;
	uint32_t	tf_r5;
	uint32_t	tf_r6;
	uint32_t	tf_r7;
	uint32_t	tf_r8;
	uint32_t	tf_r9;
	uint32_t	tf_r10;
	uint32_t	tf_r11;
	uint32_t	tf_r12;

	union {
		uint32_t	tf_r13;
		uint32_t	tf_sp;
	};

	union {
		uint32_t	tf_r14;
		uint32_t	tf_lr;
	};

	union {
		uint32_t	tf_r15;
		uint32_t	tf_pc;
	};
};

struct Trapframe_aux {
    struct thread_entry_args tfa_entry_args;
};

struct Fpregs {
};

typedef uint32_t ptent_t;

struct Pagemap {
        ptent_t pm_ent[NL1PTENTRIES];
};

struct md_Thread {
	uint32_t mt_utrap_mask;
};

extern struct Pagemap kpagemap;
#endif

#endif
