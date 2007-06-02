#ifndef JOS_MACHINE_MMU_H
#define JOS_MACHINE_MMU_H

#ifndef __ASSEMBLER__
# include <inc/types.h>
# include <inc/thread.h>
#endif

/*
 * MMU hardware
 */
#define	PGSHIFT		12
#define	PGSIZE		(1 << PGSHIFT)

/*
 * Processor status register
 */
#define	PSR_IMPL_SHIFT	28		/* CPU implementation */
#define	PSR_IMPL_MASK	0x0f
#define PSR_VER_SHIFT	24		/* CPU version */
#define PSR_VER_MASK	0x0f
#define	PSR_ICC_SHIFT	20		/* Condition codes */
#define PSR_ICC_MASK	0x0f
#define PSR_EC		(1 << 13)	/* Enable coprocessor */
#define PSR_EF		(1 << 12)	/* Enable floating-point */
#define PSR_PIL_SHIFT	8		/* Interrupt level (masking) */
#define PSR_PIL_MASK	0x0f
#define PSR_S		(1 << 7)	/* Supervisor */
#define PSR_PS		(1 << 6)	/* Previous supervisor */
#define PSR_ET		(1 << 5)	/* Enable traps */
#define PSR_CWP_SHIFT	0		/* Current window pointer */
#define PSR_CWP_MASK	0x0f

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
#endif

#endif
