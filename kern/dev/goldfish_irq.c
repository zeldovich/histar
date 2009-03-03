#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <dev/goldfish_irq.h>
#include <dev/goldfish_irqreg.h>

static struct goldfish_irqreg *irqreg;

void
goldfish_irq_init(void)
{
	irqreg = (struct goldfish_irqreg *)0xff000000;
	irqreg->disable_all = 1;
}

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
	// Don't spin forever.
	int i = 10;

	assert(irqreg != NULL);

	while (irqreg->count != 0 && irqreg->status != 0 && i > 0) {
		irq_handler(irqreg->status);
		i--;
	}
	if (i == 10)
		panic("%s:%s: spurious interrupt", __FILE__, __func__);
}
