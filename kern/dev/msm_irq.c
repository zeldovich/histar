#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/msm_irq.h>
#include <dev/msm_irqreg.h>

static struct msm_irqreg *irqreg;

#define MSM_NIRQS	64

void
msm_irq_init(uint32_t base)
{
	irqreg = (struct msm_irqreg *)base;

	// level interrupts
	irqreg->mir_type0 = irqreg->mir_type1 = 0;

	// highlevel interrupts
	irqreg->mir_polarity0 = irqreg->mir_polarity1 = 0;

	// irq (rather than fiq) for all interrupts
	irqreg->mir_select0 = irqreg->mir_select1 = 0;

	// disable interrupts
	irqreg->mir_enable0 = irqreg->mir_enable1 = 0;

	// disable 1136 vic
	irqreg->mir_config = 0;

	// enable controller (IRQs and FIQs)
	irqreg->mir_masterenable = 3;
}

#ifdef JOS_ARM_HTCDREAM
void
irq_arch_enable(uint32_t irq)
{
	assert(irqreg != NULL);
	assert(irq < MSM_NIRQS);

	cprintf("%s: enabling irq %d\n", __func__, irq);

	if (irq < 32)
		irqreg->mir_enableset0 = 1 << (irq & 31);
	else
		irqreg->mir_enableset1 = 1 << (irq & 31);
}

void
irq_arch_handle()
{
	for (int i = 0; i < 2; i++) {
		volatile uint32_t *statreg = (i == 0) ?
		    &irqreg->mir_irqstatus0 : &irqreg->mir_irqstatus1;
		volatile uint32_t *clearreg = (i == 0) ?
		    &irqreg->mir_intclear0 : &irqreg->mir_intclear1;

		// Don't spin forever.
		int j;
		for (j = 100; *statreg != 0 && j > 0; j--) {
			cprintf("irq status (set %d): 0x%08x\n", i, *statreg);

			// ack it
			*clearreg = *statreg; 
		}
		if (j == 100)
			panic("%s:%s: spurious interrupt (status0 = 0x%08x, "
			    "status1 = 0x%08x)", __FILE__, __func__,
			    irqreg->mir_irqstatus0, irqreg->mir_irqstatus1);
	}
}
#endif /* !JOS_ARM_HTCDREAM */
