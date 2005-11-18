#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/sched.h>
#include <dev/disk.h>

void
irq_handler(int irqno)
{
    switch (irqno) {
	case 0:
	    schedule();
	    break;

	case 14:
	case 15:
	    ide_intr();
	    break;

	default:
	    cprintf("IRQ %d not handled\n", irqno);
    }
}
