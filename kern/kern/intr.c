#include <machine/trapcodes.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <inc/queue.h>
#include <inc/intmacro.h>

LIST_HEAD(ih_list, interrupt_handler);
struct ih_list irq_handlers[MAX_TRAP + 1];
struct ih_list preinit_handlers;
static uint64_t irq_warnings[MAX_TRAP + 1];

static bool_t init;

void
irq_handler(uint32_t trapno)
{
    if (trapno > MAX_TRAP)
	panic("irq_handler: invalid trapno %d", trapno);

    if (LIST_FIRST(&irq_handlers[trapno]) == 0) {
	irq_warnings[trapno]++;
	if (IS_POWER_OF_2(irq_warnings[trapno]))
	    cprintf("trapno %d not handled (%"PRIu64")\n",
		    trapno, irq_warnings[trapno]);
    }

    irq_arch_eoi(trapno);

    struct interrupt_handler *ih;
    LIST_FOREACH(ih, &irq_handlers[trapno], ih_link)
	ih->ih_func(ih->ih_arg);
}

void
irq_register(struct interrupt_handler *ih)
{
    if (ih->ih_irq > MAX_IRQ)
	panic("irq_register: invalid IRQ %d", ih->ih_irq);

    if (init) {
	uint32_t tno = irq_arch_init(ih->ih_irq, ih->ih_tbdp, ih->ih_user);
	irq_arch_enable(tno);
	LIST_INSERT_HEAD(&irq_handlers[tno], ih, ih_link);
	ih->ih_trapno = tno;
    } else 
	LIST_INSERT_HEAD(&preinit_handlers, ih, ih_link);
}

void
irq_init(void)
{
    init = 1;
    struct interrupt_handler *ih;
    while ((ih = LIST_FIRST(&preinit_handlers))) {
	LIST_REMOVE(ih, ih_link);
	irq_register(ih);
    }
}
