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

/*
 * MMU hardware
 */
#define	PGSHIFT		12
#define	PGSIZE		(1 << PGSHIFT)
#define PGMASK		(PGSIZE - 1)

/* offset in page */
#define PGOFF(la)	(((uintptr_t) (la)) & PGMASK)
#define PGADDR(la)	(((uintptr_t) (la)) & ~CAST64(PGMASK))

/* Page table descriptors */
#define PTD_PTP_SHIFT	2
#define PTD_PTP_MASK	0x3fffffff
#define PTD_ET_SHIFT	0
#define PTD_ET_MASK	0x03

/* Page table entries */
#define PTE_PPN_SHIFT	8
#define PTE_PPN_MASK	0xffffff
#define PTE_C		(1 << 7)	/* Cacheable */
#define PTE_M		(1 << 6)	/* Modified */
#define PTE_R		(1 << 5)	/* Referenced */
#define PTE_ACC_SHIFT	2		/* Access permissions */
#define	PTE_ACC_MASK	0x07
#define PTE_ET_SHIFT	0		/* Entry type */
#define PTE_ET_MASK	0x03

/* Simplified view of access permission values */
#define PTE_ACC_W	(1 << 0)
#define PTE_ACC_X	(1 << 1)
#define PTE_ACC_SUPER	7		/* User none, supervisor RWX */

/* Entry types */
#define PT_ET_NONE	0
#define PT_ET_PTD	1
#define PT_ET_PTE	2

#define PTE_ADDR(e)	(((e >> PTE_PPN_SHIFT) & PTE_PPN_MASK) << PGSHIFT)

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
struct Trapcode {
    uint8_t code[16];
};

struct Trapframe_aux {
    struct thread_entry_args tfa_entry_args;
};

struct Trapframe {
    uint32_t tf_reg[32];
    uint32_t tf_psr;
    uint32_t tf_y;
    uint32_t tf_pc;
    uint32_t tf_npc;
};

struct Fpregs {
    uint32_t tf_freg[32];
    uint32_t tf_fsr;
    /* Make sure we do the right thing with %fq */
};
#endif

#endif
