#ifndef JOS_MACHINE_PSR_H
#define JOS_MACHINE_PSR_H

/* The Sparc PSR fields are laid out as the following:
 *
 *  ------------------------------------------------------------------------
 *  | impl  | vers  | icc   | resv  | EC | EF | PIL  | S | PS | ET |  CWP  |
 *  | 31-28 | 27-24 | 23-20 | 19-14 | 13 | 12 | 11-8 | 7 | 6  | 5  |  4-0  |
 *  ------------------------------------------------------------------------
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
#define PSR_PIL         (PSR_PIL_MASK << PSR_PIL_SHIFT)
#define PSR_S		(1 << 7)	/* Supervisor */
#define PSR_PS		(1 << 6)	/* Previous supervisor */
#define PSR_ET		(1 << 5)	/* Enable traps */
#define PSR_CWP_SHIFT	0		/* Current window pointer */
#define PSR_CWP_MASK	0x0f
#define PSR_CWP         (PSR_CWP_MASK << PSR_CWP_SHIFT)

#endif
