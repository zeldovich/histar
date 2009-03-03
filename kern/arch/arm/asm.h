#define ENTRY(x) \
	.text; .align 4; .globl x; .type x,%function; x:

#ifndef BIT
#define BIT(_x)	(1 << (_x))
#endif

/* 
 * CP15 - ``System Control Coprocessor''. [ARM ARM B3]
 *
 *   CP15 contains up to 16 registers that control everything from MMUs
 *   and caches to alignment constrations and exception vectors.
 *
 *   CP15 is accessed primarily via MRC and MCR read/write instructions.
 *   Some chips have MRRC and MCRR features for range reads/writes.
 *
 *   Note that each of these registers may have sub-registers addressed
 *   according to the opcode_2 field of the abovementioned instructions.
 */
#define CP15_ID_REG		0		/* ro: cpu id, cache, tlb info*/
#define CP15_CTRL_REG		1		/* rw: system cfg bits */
#define CP15_PGTBL_CTRL_REG	2		/* rw: pg tbl control */
#define CP15_DOMACC_CTRL_REG	3		/* rw: domain access control */
#define CP15_RESERVED0_REG	4
#define CP15_FAULT_STAT_REG	5		/* ro: fault status */
#define CP15_FAULT_ADDR_REG	6		/* ro: fault address */
#define CP15_CWB_CTRL_REG	7		/* rw: cache/write buffer ctrl*/
#define CP15_TLB_CTRL_REG	8		/* rw: tlb control */
#define CP15_CACHE_LOCKDN_REG	9		/* rw: cache lockdown */
#define CP15_TLB_LOCKDN_REG	10		/* rw: tlb lockdown */
#define	CP15_DMA_CTRL_REG	11		/* rw: l1 dma control */
#define CP15_RESERVED1_REG	12
#define CP15_PROCESS_ID_REG	13		/* rw: process id */
#define CP15_RESERVED3		14
#define CP15_IMPL_DEFINED_REG	15		/* implementation-defined */

/*
 * CP15_ID_REG - ``ID codes''
 *
 *   CP15_ID_REG contains 5 sub-registers with various chip information.
 */
#define ID_REG_MAIN		0x0
#define ID_REG_CACHE_TYPE	0x1
#define ID_REG_TCM_TYPE		0x2
#define ID_REG_TLB_TYPE		0x3
#define ID_REG_MPU_TYPE		0x4

/*
 * CP15_CTRL_REG - ``Control Register''. [ARM ARM B3.4]
 *
 *   CP15 contains enable/disable bits for caches, MMUs, and other
 *   miscellaneous features such as alignment constraints and exception
 *   vectors.
 *
 *   CP15_CTRL_REG contains 3 sub-registers.
 */
#define CTRL_REG_CTRL		0x0 /* Control reg */
#define CTRL_REG_AUX_CTRL	0x1 /* Aux. ctrl reg (impl-defined) */
#define CTRL_REG_COP_ACC_CTRL	0x2 /* Co-processor access control */

/* CTRL_REG_CTRL bits: */
#define CTRL_M	BIT(0)	/* 1 => MMU enabled */
#define CTRL_A	BIT(1)	/* 1 => strict alignmnt (>= v6), fault checking (< v6)*/
#define CTRL_C	BIT(2)	/* 1 => L1 unified/data cache enabled */
#define CTRL_W	BIT(3)	/* 1 => write buffer enabled */
#define CTRL_SBO (BIT(4) | BIT(5) | BIT(6))	/* reserved */
#define CTRL_B	BIT(7)	/* 1 => big endian */
#define CTRL_S	BIT(8)	/* Deprecated; See ARM ARM B4-8. */
#define CTRL_R	BIT(9)	/* Deprecated; See ARM ARM B4-8. */
#define CTRL_F	BIT(10)	/* Implementation-defined */
#define CTRL_Z	BIT(11)	/* 1 => Branch prediction enabled */
#define CTRL_I	BIT(12)	/* 1 => L1 instruction cache enabled */
#define CTRL_V	BIT(13)	/* 1 => High exception vectors (@0xffff0000,else @0x0)*/
#define CTRL_RR	BIT(14)	/* 1 => Predictable cache replacemnt strat. (rnd robn)*/
#define CTRL_L4	BIT(15)	/* Deprecated; affects thumb LDM, LDR, POP instrs */
#define CTRL_DT	BIT(16)	/* reserved */
#define CTRL_SBZ0 BIT(17) /* reads as 0, ignores writes */
#define CTRL_IT	BIT(18)	/* reserved */
#define CTRL_SBZ1 BIT(19) /* reads as 0, ignores writes */
#define CTRL_ST	BIT(20)	/* reserved */
#define CTRL_FI	BIT(21)	/* Implementation-dependant: 1=> augment FIQ config */
#define CTRL_U	BIT(22)	/* 1 => unaligned load/store & mixed endian ops OK */
#define CTRL_XP	BIT(23)	/* 1 => Subpage AP bits disabled, enable extend. pgtbl*/
#define CTRL_VE	BIT(24)	/* 1 => Implementation-dependent vectored IRQs on */
#define CTRL_EE	BIT(25)	/* 1 => big endian exceptions and page tables */
#define CTRL_L2	BIT(26)	/* 1 => unified L2 cache enabled */
/* ... the rest are reserved */

/*
 * COP_ACC_CTRL:
 */
#define COP_ACC_CTRL_DENIED		0x0	/* no access */
#define COP_ACC_CTRL_PRIVONLY		0x1	/* no user mode acccess */
#define COP_ACC_CTRL_RESERVED		0x2	/* reserved */
#define COP_ACC_CTRL_FULL		0x3	/* full access */

/* each of cp0 through cp13 have 2 bits for access rights */
#define COP_ACC_CTRL_CPn(_n, _c)	((c_) << ((_n) * 2))
