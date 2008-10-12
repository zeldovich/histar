#include <dev/ioapic.h>
#include <dev/apicreg.h>
#include <machine/mmu-x86.h>
#include <machine/pmap.h>

enum {				/* I/O APIC registers */
    IOAPIC_ID	= 0x00,		/* ID */
    IOAPIC_VER	= 0x01,		/* version */
    IOAPIC_ARB	= 0x02,		/* arbitration ID */
    IOAPIC_RDT	= 0x10,		/* redirection table */
};

void
ioapic_rdtr(struct apic* apic, uint32_t sel, uint32_t* hi, uint32_t* lo)
{
    volatile uint32_t* iowin = apic->addr + (0x10 / sizeof(uint32_t));
    sel = IOAPIC_RDT + 2 * sel;
    
    *apic->addr = sel + 1;
    if(hi)
	*hi = *iowin;
    *apic->addr = sel;
    if(lo)
	*lo = *iowin;
}

void
ioapic_rdtw(struct apic* apic, uint32_t sel, uint32_t hi, uint32_t lo)
{
    volatile uint32_t* iowin = apic->addr + (0x10 / sizeof(uint32_t));
    sel = IOAPIC_RDT + 2 * sel;

    *apic->addr = sel + 1;
    *iowin = hi;
    *apic->addr = sel;
    *iowin = lo;
}

void
ioapic_init(struct apic *apic)
{
    int r = mtrr_set(apic->paddr, PGSIZE, MTRR_BASE_UC);
    if (r < 0)
	cprintf("ioapic_init: out of MTRRs\n");

    /*
     * Initialise the I/O APIC.
     * The MultiProcessor Specification says it is the responsibility
     * of the O/S to set the APIC id.
     * Make sure interrupts are all masked off for now.
     */
    volatile uint32_t* iowin = apic->addr + (0x10 / sizeof(uint32_t));
    *apic->addr = IOAPIC_VER;
    apic->mre = (*iowin >> 16) & 0xFF;
    
    *apic->addr = IOAPIC_ID;
    *iowin = apic->apicno << 24;
    
    for(uint32_t s = 0; s <= apic->mre; s++)
	ioapic_rdtw(apic, s, 0, LAPIC_VT_MASKED);
}
