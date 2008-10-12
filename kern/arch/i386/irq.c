#include <kern/arch.h>
#include <dev/picirq.h>

uint32_t
irq_arch_enable(uint32_t irq, tbdp_t tbdp)
{
    return pic_intrenable(irq);
}

void
irq_arch_eoi(uint32_t trapno)
{
    pic_eoi();
}
