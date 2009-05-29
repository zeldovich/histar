#ifndef JOS_DEV_GOLDFISH_IRQREG
#define JOS_DEV_GOLDFISH_IRQREG

/*
 * NB: This isn't like real hardware -- when enabling/disabling
 * an IRQ, we write the IRQ _number_ to the register, _not_ the
 * bitmask.
 */
struct goldfish_irq_reg {
	volatile uint32_t count;	/* ro - # of irqs pending */
	volatile uint32_t status;	/* ro - read lowest pending irq # */
	volatile uint32_t disable_all;	/* wo - write anything to disable all irqs */
	volatile uint32_t disable;	/* wo - write irq # (not mask) to disable */
	volatile uint32_t enable;	/* wo - write irq # (not maks) to enable */
};

#endif /* !JOS_DEV_GOLDFISH_IRQREG */
