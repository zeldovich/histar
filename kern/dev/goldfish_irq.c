#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/goldfish_irq.h>
#include <dev/goldfish_irq_reg.h>

static struct goldfish_irq_reg *irqreg;

void
goldfish_irq_init(void)
{
	irqreg = (struct goldfish_irq_reg *)0xff000000;
	irqreg->disable_all = 1;
}

#ifdef JOS_ARM_GOLDFISH
void
irq_arch_enable(uint32_t irq)
{
	assert(irqreg != NULL);

	cprintf("%s: enabling irq %d\n", __func__, irq);
	irqreg->enable = irq;
}

void
irq_arch_handle()
{
	assert(irqreg != NULL);

	if (irqreg->count == 0)
		panic("%s:%s: spurious interrupt", __FILE__, __func__);

	// Don't spin forever.
	for (int i = 10; irqreg->count != 0 && i > 0; i--) {
		assert(irqreg->status != 0);
		irq_handler(irqreg->status);
	}
}
#endif /* !JOS_ARM_GOLDFISH */
