/* 
 * Copyright (C) 2004 Konrad Eisele (eiselekd@web.de), Gaisler Research
 * Copyright (C) 2004 Stefan Holst (mail@s-holst.de), Uni-Stuttgart
 */

#ifndef JOS_MACHINE_LEON_H
#define JOS_MACHINE_LEON_H

/* memory mapped leon control registers */
#define LEON_PREGS	0x80000000
#define LEON_MCFG1	0x00
#define LEON_MCFG2	0x04
#define LEON_ECTRL	0x08
#define LEON_FADDR	0x0c
#define LEON_MSTAT	0x10
#define LEON_CCTRL	0x14
#define LEON_PWDOWN	0x18
#define LEON_WPROT1	0x1C
#define LEON_WPROT2	0x20
#define LEON_LCONF	0x24
#define LEON_TCNT0	0x40
#define LEON_TRLD0	0x44
#define LEON_TCTRL0	0x48
#define LEON_TCNT1	0x50
#define LEON_TRLD1	0x54
#define LEON_TCTRL1	0x58
#define LEON_SCNT	0x60
#define LEON_SRLD	0x64
#define LEON_UART0	0x70
#define LEON_UDATA0	0x70
#define LEON_USTAT0	0x74
#define LEON_UCTRL0	0x78
#define LEON_USCAL0	0x7c
#define LEON_UART1	0x80
#define LEON_UDATA1	0x80
#define LEON_USTAT1	0x84
#define LEON_UCTRL1	0x88
#define LEON_USCAL1	0x8c
#define LEON_IMASK	0x90
#define LEON_IPEND	0x94
#define LEON_IFORCE	0x98
#define LEON_ICLEAR	0x9c
#define LEON_IOREG	0xA0
#define LEON_IODIR	0xA4
#define LEON_IOICONF	0xA8
#define LEON_IPEND2	0xB0
#define LEON_IMASK2	0xB4
#define LEON_ISTAT2	0xB8
#define LEON_ICLEAR2	0xBC

#define ASI_LEON_NOCACHE	0x01
#define ASI_LEON_DCACHE_MISS	0x01
#define ASI_LEON_CACHEREGS	0x02
#define ASI_LEON_IFLUSH		0x10
#define ASI_LEON_DFLUSH		0x11
#define ASI_LEON_MMUFLUSH	0x18
#define ASI_LEON_MMUREGS	0x19
#define ASI_LEON_BYPASS		0x1c
#define ASI_LEON_FLUSH_PAGE	0x10

/* mmu register access, ASI_LEON_MMUREGS */
#define LEON_CNR_CTRL		0x000
#define LEON_CNR_CTXP		0x100
#define LEON_CNR_CTX		0x200
#define LEON_CNR_F		0x300
#define LEON_CNR_FADDR		0x400

#define LEON_CNR_CTX_NCTX	256	/*number of MMU ctx */
#define LEON_CNR_CTRL_TLBDIS	0x80000000

#define LEON_MMUTLB_ENT_MAX	64

/*
 * diagnostic access from mmutlb.vhd:
 * 0: pte address
 * 4: pte
 * 8: additional flags
 */
#define LEON_DIAGF_LVL		0x3
#define LEON_DIAGF_WR		0x8
#define LEON_DIAGF_WR_SHIFT	3
#define LEON_DIAGF_HIT		0x10
#define LEON_DIAGF_HIT_SHIFT	4
#define LEON_DIAGF_CTX		0x1fe0
#define LEON_DIAGF_CTX_SHIFT	5
#define LEON_DIAGF_VALID	0x2000
#define LEON_DIAGF_VALID_SHIFT	13

/*
 *  Interrupt Sources
 *
 *  The interrupt source numbers directly map to the trap type and to 
 *  the bits used in the Interrupt Clear, Interrupt Force, Interrupt Mask,
 *  and the Interrupt Pending Registers.
 */
#define LEON_INTERRUPT_CORRECTABLE_MEMORY_ERROR	1
#define LEON_INTERRUPT_UART_1_RX_TX		2
#define LEON_INTERRUPT_UART_0_RX_TX		3
#define LEON_INTERRUPT_EXTERNAL_0		4
#define LEON_INTERRUPT_EXTERNAL_1		5
#define LEON_INTERRUPT_EXTERNAL_2		6
#define LEON_INTERRUPT_EXTERNAL_3		7
#define LEON_INTERRUPT_TIMER1			8
#define LEON_INTERRUPT_TIMER2			9
#define LEON_INTERRUPT_EMPTY1			10
#define LEON_INTERRUPT_EMPTY2			11
#define LEON_INTERRUPT_OPEN_ETH			12
#define LEON_INTERRUPT_EMPTY4			13
#define LEON_INTERRUPT_EMPTY5			14
#define LEON_INTERRUPT_EMPTY6			15

/* irq masks */
#define LEON_HARD_INT(x)	(1 << (x))	/* irq 0-15 */
#define LEON_IRQMASK_R		0x0000fffe	/* bit 15- 1 of lregs.irqmask */
#define LEON_IRQPRIO_R		0xfffe0000	/* bit 31-17 of lregs.irqmask */

/* leon uart register definitions */
#define LEON_OFF_UDATA	0x0
#define LEON_OFF_USTAT	0x4
#define LEON_OFF_UCTRL	0x8
#define LEON_OFF_USCAL	0xc

#define LEON_UCTRL_RE	0x01
#define LEON_UCTRL_TE	0x02
#define LEON_UCTRL_RI	0x04
#define LEON_UCTRL_TI	0x08
#define LEON_UCTRL_PS	0x10
#define LEON_UCTRL_PE	0x20
#define LEON_UCTRL_FL	0x40
#define LEON_UCTRL_LB	0x80

#define LEON_USTAT_DR	0x01
#define LEON_USTAT_TS	0x02
#define LEON_USTAT_TH	0x04
#define LEON_USTAT_BR	0x08
#define LEON_USTAT_OV	0x10
#define LEON_USTAT_PE	0x20
#define LEON_USTAT_FE	0x40

/* 
 * Memory controllers 
 */
/* ESA memory controler (used by TSIM) */
#define LEON_MCFG2_SRAMDIS		0x00002000
#define LEON_MCFG2_SDRAMEN		0x00004000
#define LEON_MCFG2_SRAMBANKSZ		0x00001e00	/* [12-9] */
#define LEON_MCFG2_SRAMBANKSZ_SHIFT	9
#define LEON_MCFG2_SDRAMBANKSZ		0x03800000	/* [25-23] */
#define LEON_MCFG2_SDRAMBANKSZ_SHIFT	23

/* Gaisler memory controller (used by Virtex2 Pro) */
#define LEON_SDCTRL_SDRAMBANKSZ		0x03800000	/* [25-23] */
#define LEON_SDCTRL_SDRAMBANKSZ_SHIFT	23

#define LEON_TCNT0_MASK	0x7fffff

#define LEON_USTAT_ERROR (LEON_USTAT_OV|LEON_USTAT_PE|LEON_USTAT_FE)	/*no break yet */

#define LEON_ETH_BASE_ADD	((unsigned long)LEON_VA_ETHERMAC)
/* map leon on ethermac adress space at pa 0xb0000000 */
#define LEON_VA_ETHERMAC	DVMA_VADDR

#ifndef __ASSEMBLER__


#endif

#endif
