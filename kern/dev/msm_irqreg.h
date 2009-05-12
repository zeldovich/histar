#ifndef JOS_DEV_MSM_IRQREG
#define JOS_DEV_MSM_IRQREG

struct msm_irqreg {
	volatile uint32_t mir_select0;		/* 0x00 */
	volatile uint32_t mir_select1;		/* 0x04 */
	volatile uint32_t _pad0[2];		/* 0x08 */

	volatile uint32_t mir_enable0;		/* 0x10 */
	volatile uint32_t mir_enable1;		/* 0x14 */
	volatile uint32_t _pad1[2];		/* 0x18 */

	volatile uint32_t mir_enableclear0;	/* 0x20 */
	volatile uint32_t mir_enableclear1;	/* 0x24 */
	volatile uint32_t _pad2[2];		/* 0x28 */

	volatile uint32_t mir_enableset0;	/* 0x30 */
	volatile uint32_t mir_enableset1;	/* 0x34 */
	volatile uint32_t _pad3[2];		/* 0x38 */

	volatile uint32_t mir_type0;		/* 0x40 */
	volatile uint32_t mir_type1;		/* 0x44 */
	volatile uint32_t _pad4[2];		/* 0x48 */

	volatile uint32_t mir_polarity0;	/* 0x50 */
	volatile uint32_t mir_polarity1;	/* 0x54 */
	volatile uint32_t _pad5[2];		/* 0x58 */

	volatile uint32_t mir_nopendingval;	/* 0x60 */
	volatile uint32_t mir_masterenable;	/* 0x64 */
	volatile uint32_t mir_config;		/* 0x68 */
	volatile uint32_t mir_protection;	/* 0x6c */

	volatile uint32_t _pad6[4];		/* 0x70 */

	volatile uint32_t mir_irqstatus0;	/* 0x80 */
	volatile uint32_t mir_irqstatus1;	/* 0x84 */
	volatile uint32_t _pad7[2];		/* 0x88 */

	volatile uint32_t mir_fiqstatus0;	/* 0x90 */
	volatile uint32_t mir_fiqstatus1;	/* 0x94 */
	volatile uint32_t _pad8[2];		/* 0x98 */

	volatile uint32_t mir_rawstatus0;	/* 0xa0 */
	volatile uint32_t mir_rawstatus1;	/* 0xa4 */
	volatile uint32_t _pad9[2];		/* 0xa8 */

	volatile uint32_t mir_intclear0;	/* 0xb0 */
	volatile uint32_t mir_intclear1;	/* 0xb4 */
	volatile uint32_t _pad10[2];		/* 0xb8 */

	volatile uint32_t mir_softint0;		/* 0xc0 */
	volatile uint32_t mir_softint1;		/* 0xc4 */
	volatile uint32_t _pad11[2];		/* 0xc8 */

	volatile uint32_t mir_irqvectread;	/* 0xd0 */
	volatile uint32_t mir_irqpendvectread;	/* 0xd4 */
	volatile uint32_t mir_irqvectwrite;	/* 0xd8 */
	volatile uint32_t _pad12;		/* 0xda */

	volatile uint32_t mir_irqinservice;	/* 0xe0 */
	volatile uint32_t mir_irqinstack;	/* 0xe4 */
	volatile uint32_t mir_testbusselect;	/* 0xe8 */
};

#endif /* !JOS_DEV_MSM_IRQREG */
