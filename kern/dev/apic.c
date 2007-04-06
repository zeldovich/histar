#include <machine/x86.h>
#include <machine/pmap.h>
#include <dev/apic.h>
#include <dev/apicreg.h>
#include <dev/picirq.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>

struct apic_preempt {
    struct preemption_timer pt;
    uint64_t freq_hz;
};

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
apic_send_ipi(uint32_t target, uint32_t vector)
{
    apic_write(LAPIC_ICRHI, target << LAPIC_ID_SHIFT);
    apic_write(LAPIC_ICRLO, LAPIC_DLMODE_FIXED | vector);

    for (uint32_t i = 0; i < (1 << 16); i++)
	if (!(apic_read(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY))
	    return;

    cprintf("apic_send_ipi: timeout\n");
}

static void
apic_schedule(void *arg, uint64_t nsec)
{
    struct apic_preempt *ap = arg;
    apic_write(LAPIC_ICR_TIMER,
	       timer_convert(nsec, ap->freq_hz, 1000000000));
}

static void
apic_intr(void *arg)
{
    apic_write(LAPIC_EOI, 0);
    schedule();
}

void
apic_init(void)
{
    int r = mtrr_set(LAPIC_BASE, PGSIZE, MTRR_BASE_UC);
    if (r < 0) {
	cprintf("apic_init: out of MTRRs\n");
	return;
    }

    uint32_t id = (apic_read(LAPIC_ID) & LAPIC_ID_MASK) >> LAPIC_ID_SHIFT;

    uint32_t v = apic_read(LAPIC_VERS);
    uint32_t vers = v & LAPIC_VERSION_MASK;
    uint32_t maxlvt = (v & LAPIC_VERSION_LVT_MASK) >> LAPIC_VERSION_LVT_SHIFT;
    cprintf("APIC: version %d, %d LVTs, APIC ID %d\n", vers, maxlvt, id);

    apic_write(LAPIC_SVR, LAPIC_SVR_FDIS | LAPIC_SVR_ENABLE | APIC_SPURIOUS);
    apic_write(LAPIC_LVINT0, LAPIC_DLMODE_EXTINT);
    apic_write(LAPIC_LVINT1, LAPIC_DLMODE_NMI);

    // Enable APIC timer for preemption.
    if (the_timesrc && !the_schedtmr) {
	static struct apic_preempt sap;
	struct apic_preempt *ap = &sap;

	apic_write(LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
	apic_write(LAPIC_ICR_TIMER, 0xffffffff);
	apic_write(LAPIC_LVTT, IRQ_OFFSET + 8);

	/* We only need this calibration to be approximate.. */
	uint64_t ccr0 = apic_read(LAPIC_CCR_TIMER);
	the_timesrc->delay_nsec(the_timesrc->arg, 10 * 1000 * 1000);
	uint64_t ccr1 = apic_read(LAPIC_CCR_TIMER);

	ap->freq_hz = (ccr0 - ccr1) * 100;
	ap->pt.arg = ap;
	ap->pt.schedule_nsec = &apic_schedule;
	the_schedtmr = &ap->pt;

	static struct interrupt_handler apic_ih = { .ih_func = &apic_intr };
	irq_register(8, &apic_ih);

	cprintf("LAPIC: %"PRIu64" Hz\n", ap->freq_hz);
    }
}
