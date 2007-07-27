#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#ifndef __ASSEMBLER__
# include <inc/types.h>
# include <inc/thread.h>
# include <inc/intmacro.h>
#else
# define UINT64(x) x
# define CAST64(x) (x)
#endif

#define NPTLVLS     2		/* page table depth -1 */
#define NPTBITS1    6
#define NPTBITS2    8

#define NPTENTRIES1     (1 << NPTBITS1)
#define NPTENTRIES2     (1 << NPTBITS2)
#define NPTENTRIES(n)   ((n) == 2 ? NPTBITS2 : NPTBITS1)

#define PDXMASK1    ((1 << NPTBITS1) - 1)
#define PDXMASK2    ((1 << NPTBITS2) - 1)
#define PDXMASK(n)  ((n) == 2 ? PDXMASK2 : PDXMASK1)

#define PDSHIFT(n)  (12 + NPTBITS1 * (n))
#define PDX(n, va)  ((((uintptr_t)(va)) >> PDSHIFT(n)) & PDXMASK(n))

/*
 * MMU hardware
 */
#define	PGSHIFT		12
#define PGSIZE		(1 << PGSHIFT)
#define PGMASK		(PGSIZE - 1)

#define PTPSHIFT        6

/* offset in page */
#define PGOFF(la)	(((uintptr_t) (la)) & PGMASK)
#define PGADDR(la)	(((uintptr_t) (la)) & ~CAST64(PGMASK))

/* Page table descriptors */
#define PTD_PTP_SHIFT	2
#define PTD_PTP_MASK	0x3fffffff

/* Page table entries */
#define PTE_PPN_SHIFT	8
#define PTE_PPN_MASK	0xffffff
#define PTE_C		(1 << 7)	/* Cacheable */
#define PTE_M		(1 << 6)	/* Modified */
#define PTE_R		(1 << 5)	/* Referenced */
#define PTE_ACC_SHIFT	2		/* Access permissions */
#define	PTE_ACC_MASK	0x07
#define PTE_ACC(pte)    (((pte) >> PTE_ACC_SHIFT) & PTE_ACC_MASK)

/* Common to PTD and PTE */
#define PT_ET_SHIFT	0		/* Entry type */
#define PT_ET_MASK	0x03
#define PT_ET(e)        (((e) >> PT_ET_SHIFT) & PT_ET_MASK)

/* Simplified view of access permission values */
#define PTE_ACC_W	(1 << 0)
#define PTE_ACC_X	(1 << 1)
#define PTE_ACC_SUPER	7		/* User none, supervisor RWX */

/* Entry types */
#define PT_ET_NONE	0
#define PT_ET_PTD	1
#define PT_ET_PTE	2

#define PTE_ADDR(e)	(((e >> PTE_PPN_SHIFT) & PTE_PPN_MASK) << PGSHIFT)
#define PTD_ADDR(e)     (((e >> PTD_PTP_SHIFT) & PTD_PTP_MASK) << PTPSHIFT)

#define PTD_ENTRY(pa)   ((((pa) >> PTPSHIFT) << PTD_PTP_SHIFT) | PT_ET_PTD)
#define PTE_ENTRY(pa, flags) ((((pa) >> PGSHIFT) << PTE_PPN_SHIFT) | \
                             (PT_ET_PTE << PT_ET_SHIFT) |            \
                             ((flags) << PTE_ACC_SHIFT) |            \
                             PTE_C)

/* SRMMU Register addresses in ASI_MMUREGS */
#define SRMMU_CTRL_REG           0x00000000
#define SRMMU_CTXTBL_PTR         0x00000100
#define SRMMU_CTX_REG            0x00000200
#define SRMMU_FAULT_STATUS       0x00000300
#define SRMMU_FAULT_ADDR         0x00000400

#define SRMMU_CTRL_E       0x00000001  /* enable MMU bit */
#define SRMMU_CTRL_NF      0x00000020  /* no fault bit */

/* Fault Status Register fields */
/* access type */
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
/* fault type */
#define SRMMU_FT_SHIFT     2
#define SRMMU_FT_MASK      0x07


/*
 * Trap base register 
 */
#define TBR_TT_SHIFT	4		/* Trap type */
#define TBR_TT_MASK	0xff

/*
 * Floating-point status/control register
 */
#define FSR_RD_SHIFT	30		/* Rounding direction */
#define FSR_RD_MASK	0x03
#define FSR_TEM_SHIFT	23		/* Trap enable mask */
#define FSR_TEM_MASK	0x1f
#define FSR_NS		(1 << 22)	/* Non-standard */
#define FSR_VER_SHIFT	17		/* Version */
#define FSR_VER_MASK	0x07
#define FSR_FTT_SHIFT	14		/* Trap type */
#define FSR_FTT_MASK	0x07
#define FSR_QNE		(1 << 13)	/* Queue non-empty */
#define FSR_FCC_SHIFT	10		/* FPU condition codes */
#define FSR_FCC_MASK	0x03
#define FSR_AX_SHIFT	5		/* Accured exception */
#define FSR_AX_MASK	0x1f
#define FSR_CX_SHIFT	0		/* Current exception */
#define FSR_CX_MASK	0x1f

#ifndef __ASSEMBLER__

struct Regs {
    uint32_t g0;
    uint32_t g1;
    uint32_t g2;
    uint32_t g3;
    uint32_t g4;
    uint32_t g5;
    uint32_t g6;
    uint32_t g7;

    uint32_t o0;
    uint32_t o1;
    uint32_t o2;
    uint32_t o3;
    uint32_t o4;
    uint32_t o5;
    union {
	uint32_t o6;
	uint32_t sp;
    };
    uint32_t o7;
    
    uint32_t l0;
    uint32_t l1;
    uint32_t l2;
    uint32_t l3;
    uint32_t l4;
    uint32_t l5;
    uint32_t l6;
    uint32_t l7;

    uint32_t i0;
    uint32_t i1;
    uint32_t i2;
    uint32_t i3;
    uint32_t i4;
    uint32_t i5;
    union {
	uint32_t i6;
	uint32_t fp;
    };
    uint32_t i7;
};

struct Fpregs {
    uint32_t tf_freg[32];
    uint32_t tf_fsr;
    /* Make sure we do the right thing with %fq */
};

struct Trapcode {
    uint8_t code[16];
};

struct Trapframe_aux {
    struct thread_entry_args tfa_entry_args;
};

struct Trapframe {
    union {
	uint32_t tf_reg0[32];
	struct Regs tf_reg1;
    };
    uint32_t tf_psr;
    uint32_t tf_y;
    uint32_t tf_pc;
    uint32_t tf_npc;
};

#else

/* Reg_window offsets */
#define RW_L0     0x00
#define RW_L1     0x04
#define RW_L2     0x08
#define RW_L3     0x0c
#define RW_L4     0x10
#define RW_L5     0x14
#define RW_L6     0x18
#define RW_L7     0x1c
#define RW_I0     0x20
#define RW_I1     0x24
#define RW_I2     0x28
#define RW_I3     0x2c
#define RW_I4     0x30
#define RW_I5     0x34
#define RW_I6     0x38
#define RW_I7     0x3c

#define STACKFRAME_SZ 0x40

/* Trapframe offsets */
#define TF_G0     0x00
#define TF_G1     0x04
#define TF_G2     0x08
#define TF_G3     0x0C
#define TF_G4     0x10
#define TF_G5     0x14
#define TF_G6     0x18
#define TF_G7     0x1C

#define TF_O0     0x20
#define TF_O1     0x24
#define TF_O2     0x28
#define TF_O3     0x2C
#define TF_O4     0x30
#define TF_O5     0x34
#define TF_O6     0x38
#define TF_SP     0x38
#define TF_O7     0x3C

#define TF_L0     0x40
#define TF_L1     0x44
#define TF_L2     0x48
#define TF_L3     0x4C
#define TF_L4     0x50
#define TF_L5     0x54
#define TF_L6     0x58
#define TF_L7     0x5C

#define TF_I0     0x60
#define TF_I1     0x64
#define TF_I2     0x68
#define TF_I3     0x6C
#define TF_I4     0x70
#define TF_I5     0x74
#define TF_I6     0x78
#define TF_I7     0x7C

#define TF_PSR    0x80
#define TF_Y      0x84
#define TF_PC     0x88
#define TF_NPC    0x8C

#define TRAPFRAME_SZ 0x90

#endif

#endif
