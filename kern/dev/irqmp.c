#include <kern/lib.h>
#include <kern/arch.h>

#include <machine/leon3.h>
#include <machine/leon.h>

#include <dev/irqmp.h>
#include <dev/amba.h>
#include <dev/ambapp.h>

static LEON3_IrqCtrl_Regs_Map *irq_regs;

void
irq_arch_enable(uint32_t irqno)
{
    assert(irq_regs);
    assert(irqno > 0 && irqno < MAX_IRQS);
    uint32_t m = irq_regs->mask[0];
    m |= (1 << irqno);
    irq_regs->mask[0] = m;
}

void
irqmp_clear(uint32_t irqno)
{
    uint32_t c = (1 << irqno);
    irq_regs->iclear = c;
}

void 
irqmp_init(void)
{
    struct amba_apb_device dev;
    uint32_t r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_IRQMP, &dev, 0);
    if (!r)
	return;

    irq_regs = pa2kva(dev.start);
    if (!irq_regs) {
	cprintf("irqmp_init: unable to find irq cntrl registers\n");
	return;
    }
    
    irq_regs->mask[0] = 0;
    irq_regs->iclear = ~0;
}
