#include <linux/irq.h>
#include <linux/interrupt.h>

/* hw_interrupt_type must define (startup || enable) &&
 * (shutdown || disable) && end */
static void dummy(unsigned int irq)
{
}

/* This is used for everything else than the timer. */
static struct hw_interrupt_type normal_irq_type = {
	.typename = "SIGIO",
	.disable = dummy,
	.enable = dummy,
	.ack = dummy,
	.end = dummy
};

static struct hw_interrupt_type SIGVTALRM_irq_type = {
	.typename = "SIGVTALRM",
	.shutdown = dummy, /* never called */
	.disable = dummy,
	.enable = dummy,
	.ack = dummy,
	.end = dummy
};

void 
init_IRQ(void)
{
	int i;

	irq_desc[LIND_TIMER_IRQ].status = IRQ_DISABLED;
	irq_desc[LIND_TIMER_IRQ].action = NULL;
	irq_desc[LIND_TIMER_IRQ].depth = 1;
	irq_desc[LIND_TIMER_IRQ].chip = &SIGVTALRM_irq_type;
	enable_irq(LIND_TIMER_IRQ);
	for (i = 1; i < NR_IRQS; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		irq_desc[i].chip = &normal_irq_type;
		enable_irq(i);
	}
}
