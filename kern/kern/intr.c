#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/sched.h>

void
irq_handler(int irqno)
{
    switch (irqno) {
	case 0:
	    schedule();
	    break;

	default:
	    cprintf("IRQ %d not handled\n", irqno);
    }
}
