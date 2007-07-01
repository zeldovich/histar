#include <kern/lib.h>

#include <machine/ambapp.h>
#include <machine/leon3.h>
#include <machine/leon.h>

#include <dev/irqmp.h>
#include <dev/amba.h>

void 
irqmp_init(void)
{
    struct amba_apb_device dev;
    uint32_t r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_IRQMP, &dev, 0);
    if (!r)
	return;

    LEON3_IrqCtrl_Regs_Map *irq_regs = (LEON3_IrqCtrl_Regs_Map *)dev.start;
    if (!irq_regs) {
	cprintf("irqmp_init: unable to find irq cntrl registers\n");
	return;
    }
    
    /* XXX */
    LEON_BYPASS_STORE_PA(&(irq_regs->mask[0]), 0);
}
