#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/timer.h>
#include <dev/disk.h>
#include <dev/console.h>
#include <dev/picirq.h>
#include <dev/fxp.h>

void
irq_handler(int irqno)
{
    irq_eoi(irqno);

    switch (irqno) {
	case 0:
	    timer_intr();
	    break;

	case 1:
	    kbd_intr();
	    break;

	case 4:
	    serial_intr();
	    break;

	case 5:
	    fxp_intr();
	    break;

	case 7:
	    lpt_intr();
	    break;

	case 14:
	case 15:
	    ide_intr();
	    break;

	default:
	    cprintf("IRQ %d not handled\n", irqno);
    }
}
