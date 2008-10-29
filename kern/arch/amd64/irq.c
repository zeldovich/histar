#include <machine/mp.h>
#include <kern/arch.h>
#include <dev/picirq.h>
#include <dev/apic.h>

enum { use_ioapic = 1 };

uint32_t
irq_arch_enable(uint32_t irq, tbdp_t tbdp)
{
    /* Assume apic code set local interrupts */
    if(irq >= IRQ_LINT0 && irq <= MAX_IRQ_LAPIC)
	return T_PIC + irq;
    if (use_ioapic)
	return mp_intrenable(irq, tbdp);
    return pic_intrenable(irq);
}

void
irq_arch_disable(uint32_t trapno)
{
    if (!use_ioapic)
	panic("not supported w/o IOAPIC");
    if (trapno >= T_LAPIC && trapno <= T_LAPIC + MAX_IRQ_LAPIC)
	panic("APIC trapno %u not supported", trapno);

    mp_intrdisable(trapno);
}

void
irq_arch_eoi(uint32_t trapno)
{
    uint32_t x = trapno - T_PIC;
    if (x == IRQ_SPURIOUS)
	return;
    if((x >= IRQ_LINT0 && x <= MAX_IRQ_LAPIC) || use_ioapic)
	apic_eoi();
    else
	pic_eoi();
}
