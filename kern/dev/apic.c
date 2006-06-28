#include <machine/x86.h>
#include <machine/pmap.h>
#include <dev/apic.h>
#include <dev/apicreg.h>

static uint32_t
apic_read(uint32_t off)
{
    return *(volatile uint32_t *) (pa2kva(LAPIC_BASE) + off);
}

static void
apic_write(uint32_t off, uint32_t val)
{
    *(volatile uint32_t *) (pa2kva(LAPIC_BASE) + off) = val;
}

void
apic_init(void)
{
    mtrr_set(LAPIC_BASE, PGSIZE, MTRR_BASE_UC);

    uint32_t v = apic_read(LAPIC_VERS);
    uint32_t vers = v & LAPIC_VERSION_MASK;
    uint32_t maxlvt = (v & LAPIC_VERSION_LVT_MASK) >> LAPIC_VERSION_LVT_SHIFT;
    cprintf("APIC: version %d, %d LVTs\n", vers, maxlvt);

    apic_write(LAPIC_SVR, LAPIC_SVR_FDIS | LAPIC_SVR_ENABLE | APIC_SPURIOUS);
    apic_write(LAPIC_LVINT0, LAPIC_DLMODE_EXTINT);
    apic_write(LAPIC_LVINT1, LAPIC_DLMODE_NMI);

    // Enable APIC timer
    apic_write(LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
    apic_write(LAPIC_ICR_TIMER, 0xfff);
    apic_write(LAPIC_LVTT, LAPIC_LVTT_TM | 0xfe);
}
