#include <kern/intr.h>
#include <kern/lib.h>
#include <dev/picirq.h>
#include <inc/queue.h>

LIST_HEAD(ih_list, interrupt_handler);
struct ih_list irq_handlers[MAX_IRQS];

void
irq_handler(int irqno)
{
    if (irqno < 0 || irqno >= MAX_IRQS)
	panic("irq_handler: invalid IRQ %d", irqno);

    irq_eoi(irqno);

    if (LIST_FIRST(&irq_handlers[irqno]) == 0)
	cprintf("IRQ %d not handled\n", irqno);

    struct interrupt_handler *ih;
    LIST_FOREACH(ih, &irq_handlers[irqno], ih_link)
	ih->ih_func();
}

void
irq_register(int irq, struct interrupt_handler *ih)
{
    if (irq < 0 || irq >= MAX_IRQS)
	panic("irq_register: invalid IRQ %d", irq);

    LIST_INSERT_HEAD(&irq_handlers[irq], ih, ih_link);
    irq_setmask_8259A(irq_mask_8259A & ~(1 << irq));
}
