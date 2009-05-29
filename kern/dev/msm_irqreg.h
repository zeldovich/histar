#ifndef JOS_DEV_MSM_IRQREG
#define JOS_DEV_MSM_IRQREG

/* MSM7201A register layout */
struct msm_irq_reg {
	// Interrupt type bitmask: one bit for each irq, 1 => FIQ, 0 => IRQ.
	volatile uint32_t vicintselect_0;	/* 0x00 */
	volatile uint32_t vicintselect_1;	/* 0x04 */
	volatile uint32_t _pad0[2];		/* 0x08 */

	// Bitmask of enabled interrupts. Use vicintenclear_X and
	// vicintenset_X to mask and unmask interrupts, respectively.
	// NB: Do not confuse with vicintclear_X!
	volatile uint32_t vicinten_0;		/* 0x10 */
	volatile uint32_t vicinten_1;		/* 0x14 */
	volatile uint32_t _pad1[2];		/* 0x18 */

	// Disable individual interrupts: 1 => disable/mask corresponding irq.
	volatile uint32_t vicintenclear_0;	/* 0x20 */
	volatile uint32_t vicintenclear_1;	/* 0x24 */
	volatile uint32_t _pad2[2];		/* 0x28 */

	// Enable individual interrupts: 1 => enable/unmask corresponding irq.
	volatile uint32_t vicintenset_0;	/* 0x30 */
	volatile uint32_t vicintenset_1;	/* 0x34 */
	volatile uint32_t _pad3[2];		/* 0x38 */

	// Interrupt source bitmask: one bit for each irq, 1 => edge-triggered,
	// 0 => level triggered.
	volatile uint32_t vicinttype_0;		/* 0x40 */
	volatile uint32_t vicinttype_1;		/* 0x44 */
	volatile uint32_t _pad4[2];		/* 0x48 */

	// Interrupt polarity bitmask: one bit for each irq, 1 => negative,
	// 0 => positive. 
	volatile uint32_t vicintpolarity_0;	/* 0x50 */
	volatile uint32_t vicintpolarity_1;	/* 0x54 */
	volatile uint32_t _pad5[2];		/* 0x58 */

	// ??
	volatile uint32_t vicno_pend_val;	/* 0x60 */

	// Master switch: 0x1 => enable IRQs, 0x2 => enable FIQs 
	volatile uint32_t vicintmasteren;	/* 0x64 */

	// 0x1 => enable ARM1136 VIC port
	volatile uint32_t vicconfig;		/* 0x68 */

	// 0x1 => hprot used (only privileged transactions, rather than user,
	// in the VIC slave).
	volatile uint32_t vicprotection;	/* 0x6c */
	volatile uint32_t _pad6[4];		/* 0x70 */

	// One bit for each IRQ, set as follows:
	//     vicirq_status_X = vicraw_status_X & vicenable_X & ~vicintselect_X
	volatile uint32_t vicirq_status_0;	/* 0x80 */
	volatile uint32_t vicirq_status_1;	/* 0x84 */
	volatile uint32_t _pad7[2];		/* 0x88 */

	// One bit for each IRQ, set as follows:
	//     vicfiq_status_X = vicraw_status_X & vicenable_X & vicintselect_X 
	volatile uint32_t vicfiq_status_0;	/* 0x90 */
	volatile uint32_t vicfiq_status_1;	/* 0x94 */
	volatile uint32_t _pad8[2];		/* 0x98 */

	// One bit for each IRQ, always set if interrupt occurred (i.e. not
	// masked by vicinten_X nor vicintselect_X).
	volatile uint32_t vicraw_status_0;	/* 0xa0 */
	volatile uint32_t vicraw_status_1;	/* 0xa4 */
	volatile uint32_t _pad9[2];		/* 0xa8 */

	// Write to clear pending interrupts in vic{irq,fiq}_status_X.
	// 1 => clear, 0 => leave be.
	// NB: Do not confuse with vicintenclear_X!
	volatile uint32_t vicintclear_0;	/* 0xb0 */
	volatile uint32_t vicintclear_1;	/* 0xb4 */
	volatile uint32_t _pad10[2];		/* 0xb8 */

	// Test registers. Setting bits to 1 triggers an interrupt and
	// asserts the corresponding bits in vic{irq,fiq}_status_X registers.
	volatile uint32_t vicsoftint_0;		/* 0xc0 */
	volatile uint32_t vicsoftint_1;		/* 0xc4 */
	volatile uint32_t _pad11[2];		/* 0xc8 */

	// ??
	volatile uint32_t vicirq_vec_rd;	/* 0xd0 */
	volatile uint32_t vicirq_vec_pend_rd;	/* 0xd4 */
	volatile uint32_t vicirq_vec_wr;	/* 0xd8 */
	volatile uint32_t _pad12;		/* 0xda */

	// ??
	volatile uint32_t vicirq_in_service;	/* 0xe0 */
	volatile uint32_t vicirq_in_stack;	/* 0xe4 */
	volatile uint32_t victest_bus_sel;	/* 0xe8 */
};

#endif /* !JOS_DEV_MSM_IRQREG */
