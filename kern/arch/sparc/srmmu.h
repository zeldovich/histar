#ifndef JOS_MACHINE_SRMMU_H
#define JOS_MACHINE_SRMMU_H

#include <machine/sparc-config.h>

/* Number of contexts is implementation-dependent */
#define SRMMU_MAX_CONTEXTS	CTX_NCTX

/* PMD_SHIFT determines the size of the area a second-level page table entry can map */
#define SRMMU_REAL_PMD_SHIFT		18
#define SRMMU_REAL_PMD_SIZE		(1UL << SRMMU_REAL_PMD_SHIFT)
#define SRMMU_REAL_PMD_MASK		(~(SRMMU_REAL_PMD_SIZE-1))
#define SRMMU_REAL_PMD_ALIGN(__addr)	(((__addr)+SRMMU_REAL_PMD_SIZE-1)&SRMMU_REAL_PMD_MASK)

/* PGDIR_SHIFT determines what a third-level page table entry can map */
#define SRMMU_PGDIR_SHIFT       24
#define SRMMU_PGDIR_SIZE        (1UL << SRMMU_PGDIR_SHIFT)
#define SRMMU_PGDIR_MASK        (~(SRMMU_PGDIR_SIZE-1))
#define SRMMU_PGDIR_ALIGN(addr) (((addr)+SRMMU_PGDIR_SIZE-1)&SRMMU_PGDIR_MASK)

#define SRMMU_REAL_PTRS_PER_PTE	64
#define SRMMU_REAL_PTRS_PER_PMD	64
#define SRMMU_PTRS_PER_PGD	256

#define SRMMU_REAL_PTE_TABLE_SIZE	(SRMMU_REAL_PTRS_PER_PTE*4)
#define SRMMU_PMD_TABLE_SIZE		(SRMMU_REAL_PTRS_PER_PMD*4)
#define SRMMU_PGD_TABLE_SIZE		(SRMMU_PTRS_PER_PGD*4)

/*
 * To support pagetables in highmem, Linux introduces APIs which
 * return struct page* and generally manipulate page tables when
 * they are not mapped into kernel space. Our hardware page tables
 * are smaller than pages. We lump hardware tabes into big, page sized
 * software tables.
 *
 * PMD_SHIFT determines the size of the area a second-level page table entry
 * can map, and our pmd_t is 16 times larger than normal.  The values which
 * were once defined here are now generic for 4c and srmmu, so they're
 * found in pgtable.h.
 */
#define SRMMU_PTRS_PER_PMD	4

/* Definition of the values in the ET field of PTD's and PTE's */
#define SRMMU_ET_MASK         0x3
#define SRMMU_ET_INVALID      0x0
#define SRMMU_ET_PTD          0x1
#define SRMMU_ET_PTE          0x2
#define SRMMU_ET_REPTE        0x3 /* AIEEE, SuperSparc II reverse endian page! */

/* Physical page extraction from PTP's and PTE's. */
#define SRMMU_CTX_PMASK    0xfffffff0
#define SRMMU_PTD_PMASK    0xfffffff0
#define SRMMU_PTE_PMASK    0xffffff00

/* The pte non-page bits.  Some notes:
 * 1) cache, dirty, valid, and ref are frobbable
 *    for both supervisor and user pages.
 * 2) exec and write will only give the desired effect
 *    on user pages
 * 3) use priv and priv_readonly for changing the
 *    characteristics of supervisor ptes
 */
#define SRMMU_CACHE        0x80
#define SRMMU_DIRTY        0x40
#define SRMMU_REF          0x20
#define SRMMU_NOREAD       0x10
#define SRMMU_EXEC         0x08
#define SRMMU_WRITE        0x04
#define SRMMU_VALID        0x02 /* SRMMU_ET_PTE */
#define SRMMU_PRIV         0x1c
#define SRMMU_PRIV_RDONLY  0x18

#define SRMMU_FILE         0x40	/* Implemented in software */

#define SRMMU_PTE_FILE_SHIFT     8	/* == 32-PTE_FILE_MAX_BITS */

#define SRMMU_CHG_MASK    (0xffffff00 | SRMMU_REF | SRMMU_DIRTY)

/* SRMMU swap entry encoding
 *
 * We use 5 bits for the type and 19 for the offset.  This gives us
 * 32 swapfiles of 4GB each.  Encoding looks like:
 *
 * oooooooooooooooooootttttRRRRRRRR
 * fedcba9876543210fedcba9876543210
 *
 * The bottom 8 bits are reserved for protection and status bits, especially
 * FILE and PRESENT.
 */
#define SRMMU_SWP_TYPE_MASK	0x1f
#define SRMMU_SWP_TYPE_SHIFT	SRMMU_PTE_FILE_SHIFT
#define SRMMU_SWP_OFF_MASK	0x7ffff
#define SRMMU_SWP_OFF_SHIFT	(SRMMU_PTE_FILE_SHIFT + 5)

/* Some day I will implement true fine grained access bits for
 * user pages because the SRMMU gives us the capabilities to
 * enforce all the protection levels that vma's can have.
 * XXX But for now...
 */
#define SRMMU_PAGE_NONE    __pgprot(SRMMU_CACHE | \
				    SRMMU_PRIV | SRMMU_REF)
#define SRMMU_PAGE_SHARED  __pgprot(SRMMU_VALID | SRMMU_CACHE | \
				    SRMMU_EXEC | SRMMU_WRITE | SRMMU_REF)
#define SRMMU_PAGE_COPY    __pgprot(SRMMU_VALID | SRMMU_CACHE | \
				    SRMMU_EXEC | SRMMU_REF)
#define SRMMU_PAGE_RDONLY  __pgprot(SRMMU_VALID | SRMMU_CACHE | \
				    SRMMU_EXEC | SRMMU_REF)
#define SRMMU_PAGE_KERNEL  __pgprot(SRMMU_VALID | SRMMU_CACHE | SRMMU_PRIV | \
				    SRMMU_DIRTY | SRMMU_REF)

/* SRMMU Register addresses in ASI 0x4.  These are valid for all
 * current SRMMU implementations that exist.
 */
#define SRMMU_CTRL_REG           0x00000000
#define SRMMU_CTXTBL_PTR         0x00000100
#define SRMMU_CTX_REG            0x00000200
#define SRMMU_FAULT_STATUS       0x00000300
#define SRMMU_FAULT_ADDR         0x00000400

#define SRMMU_CTRL_E       0x00000001
#define SRMMU_CTRL_NF      0x00000020

/* Fault Status Register: access type */
#define SRMMU_AT_SHIFT     5
#define SRMMU_AT_MASK      0x07
#define SRMMU_AT_LUD       0      /* load user data */
#define SRMMU_AT_LSD       1      /* load supervisor data */
#define SRMMU_AT_LUI       2      /* load/exec user instruction */
#define SRMMU_AT_LSI       3      /* load/exec supervisor instruction */
#define SRMMU_AT_SUD       4      /* store user data */
#define SRMMU_AT_SSD       5      /* store supervisor data */
#define SRMMU_AT_SUI       6      /* store user instruction */
#define SRMMU_AT_SSI       7      /* store supervisor instruction */

#endif
