#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/msm_irq.h>
#include <dev/msm_irqreg.h>
#include <dev/msm_gpio.h>

static struct msm_irq_reg *irqreg;

void
msm_irq_init(uint32_t base)
{
	irqreg = (struct msm_irq_reg *)base;

	irqreg->vicinttype_0     = irqreg->vicinttype_1 = 0;
	irqreg->vicintpolarity_0 = irqreg->vicintpolarity_1 = 0;
	irqreg->vicintselect_0   = irqreg->vicintselect_1 = 0;
	irqreg->vicconfig        = 0;
	irqreg->vicintenclear_0	 = irqreg->vicintenclear_1 = 0xffffffff;
	irqreg->vicintclear_0    = irqreg->vicintclear_1 = 0xffffffff;
	irqreg->vicintmasteren   = 0x3;
}

#ifdef JOS_ARM_HTCDREAM
static bool_t
is_edge_irq(uint32_t irq)
{
	//XXX- undoubtedly not complete
	return (irq == 0 || irq == 5 || irq == 7 || irq == 8);
}

void
irq_arch_enable(uint32_t irq)
{
	assert(irqreg != NULL);
	assert(irq < MSM_NIRQS);

	cprintf("%s: enabling irq %d\n", __func__, irq);

	if (irq >= MSM_NIRQS) {
		msm_gpio_irq_enable(irq);
	} else {
		if (irq < 32) {
			if (is_edge_irq(irq))
				irqreg->vicinttype_0 |= (1U << (irq & 31));
			irqreg->vicintenset_0 = 1U << (irq & 31);
		} else {
			if (is_edge_irq(irq))
				irqreg->vicinttype_1 |= (1U << (irq & 31));
			irqreg->vicintenset_1 = 1U << (irq & 31);
		}
	}
}

void
irq_arch_handle()
{
	uint32_t s0, s1;

	assert(irqreg != NULL);

	if ((s0 = irqreg->vicirq_status_0) != 0) {
		uint32_t clearval = 0;
		for (int k = 0; k < 32; k++) {
			if (s0 & (1U << k)) {
				// handle irq and ACK it
				irq_handler(k);
				clearval |= (1U << k);
			}
		}
		irqreg->vicintclear_0 |= clearval;
	}
	if ((s1 = irqreg->vicirq_status_1) != 0) {
		uint32_t clearval = 0;
		for (int k = 0; k < 32; k++) {
			if (s1 & (1U << k)) {
				// handle irq and ACK it
				irq_handler(k + 32);
				clearval |= (1U << k);
			}
		}
		irqreg->vicintclear_1 |= clearval;
	}

	/* call the gpio interrupt handler, since it's not chained off of VIC */
	msm_gpio_irq_handler();
}
#endif /* !JOS_ARM_HTCDREAM */
