#include <machine/x86.h>
#include <machine/mp.h>
#include <machine/trapcodes.h>
#include <machine/io.h>
#include <dev/apic.h>
#include <dev/apicreg.h>
#include <dev/picirq.h>
#include <dev/nvram.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/arch.h>
#include <kern/lib.h>

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

static void
apic_icr_wait()
{
    for (uint32_t i = 0; i < (1 << 16); i++)
	if (!(apic_read(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY))
	    return;

    cprintf("apic_icr_wait: timeout\n");
}

void
apic_send_ipi(uint32_t target, uint32_t vector)
{
    apic_write(LAPIC_ICRHI, target << LAPIC_ID_SHIFT);
    apic_write(LAPIC_ICRLO, LAPIC_DLMODE_FIXED | vector);
    apic_icr_wait();
}

static int
ipi_init(uint32_t apicid)
{
    // Intel MultiProcessor spec. section B.4.1
    apic_write(LAPIC_ICRHI, apicid << LAPIC_ID_SHIFT);
    apic_write(LAPIC_ICRLO, apicid | LAPIC_DLMODE_INIT | LAPIC_LVL_TRIG |
	       LAPIC_LVL_ASSERT);
    apic_icr_wait();
    timer_delay(10 * 1000000);	// 10ms

    apic_write(LAPIC_ICRLO, apicid | LAPIC_DLMODE_INIT | LAPIC_LVL_TRIG |
	       LAPIC_LVL_DEASSERT);
    apic_icr_wait();

    return (apic_read(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY) ? -1 : 0;
}

void
apic_start_ap(uint32_t apicid, physaddr_t pa)
{
    // Universal Start-up Algorithm from Intel MultiProcessor spec
    int r;
    uint16_t *dwordptr;

    // "The BSP must initialize CMOS shutdown code to 0Ah ..."
    outb(IO_RTC, NVRAM_RESET);
    outb(IO_RTC + 1, NVRAM_RESET_JUMP);

    // "and the warm reset vector (DWORD based at 40:67) to point
    // to the AP startup code ..."
    dwordptr = pa2kva(0x467);
    dwordptr[0] = 0;
    dwordptr[1] = pa >> 4;

    // ... prior to executing the following sequence:"
    if ((r = ipi_init(apicid)) < 0)
	panic("unable to send init");
    timer_delay(10 * 1000000);	// 10ms

    for (uint32_t i = 0; i < 2; i++) {
	apic_icr_wait();
	apic_write(LAPIC_ICRHI, apicid << LAPIC_ID_SHIFT);
	apic_write(LAPIC_ICRLO, LAPIC_DLMODE_STARTUP | (pa >> 12));
	timer_delay(200 * 1000);	// 200us
    }
}

static void __attribute__((unused))
apic_print_error(void)
{
    static const char *error[8] = {
	"Send checksum error",
	"Recieve checksum error",
	"Send accept error",
	"Recieve accept error",
	"Reserved",
	"Send illegal vector",
	"Recieve illegal vector",
	"Illegal register address"
    };

    char header = 0;

    // write once to reload ESR
    apic_write(LAPIC_ESR, 0);
    uint32_t e = apic_read(LAPIC_ESR);

    for (uint32_t i = 0; i < 8; i++) {
	if (i == 4)
	    continue;
	if (e & (1 << i)) {
	    if (!header) {
		header = 1;
		cprintf("apic error:\n");
	    }
	    cprintf(" %s\n", error[i]);
	}
    }
}

void
apic_eoi(void)
{
    apic_write(LAPIC_EOI, 0);
}

void
apic_set_tpr(uint8_t priority)
{
    assert(priority <= 15);
    apic_write(LAPIC_TPRI, LAPIC_TPRI_MASK & (priority << 4));
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
    if (r < 0)
	return;

    uint32_t id = (apic_read(LAPIC_ID) & LAPIC_ID_MASK) >> LAPIC_ID_SHIFT;

    uint32_t v = apic_read(LAPIC_VERS);
    uint32_t vers = v & LAPIC_VERSION_MASK;
    uint32_t maxlvt = (v & LAPIC_VERSION_LVT_MASK) >> LAPIC_VERSION_LVT_SHIFT;
    cprintf("APIC: version %d, %d LVTs, APIC ID %d\n", vers, maxlvt, id);

    apic_write(LAPIC_SVR, LAPIC_SVR_FDIS | LAPIC_SVR_ENABLE | APIC_SPURIOUS);

    if (&cpus[arch_cpu()] == bcpu) {
	apic_write(LAPIC_LVINT0, LAPIC_DLMODE_EXTINT | APIC_TRAPNO(IRQ_LINT0));
	apic_write(LAPIC_LVINT1, LAPIC_DLMODE_NMI | APIC_TRAPNO(IRQ_LINT1));
    } else {
	apic_write(LAPIC_LVINT0, LAPIC_DLMODE_EXTINT | APIC_TRAPNO(IRQ_LINT0) | LAPIC_LVT_MASKED);
	apic_write(LAPIC_LVINT1, LAPIC_DLMODE_NMI | APIC_TRAPNO(IRQ_LINT0) | LAPIC_LVT_MASKED);
    }

    if (((v >> LAPIC_VERSION_LVT_SHIFT) & 0x0FF) >= 4)
	apic_write(LAPIC_PCINT, LAPIC_LVT_MASKED | APIC_TRAPNO(IRQ_PCINT));

    apic_write(LAPIC_LVERR, APIC_TRAPNO(IRQ_ERROR));

    // Clear error status register (requires back-to-back writes).
    apic_write(LAPIC_ESR, 0);
    apic_write(LAPIC_ESR, 0);

    // Send an Init Level De-Assert to synchronise arbitration ID's.
    apic_write(LAPIC_ICRHI, 0);
    apic_write(LAPIC_ICRLO, LAPIC_DEST_ALLINCL | LAPIC_DLMODE_INIT |
	       LAPIC_LVL_TRIG | LAPIC_LVL_DEASSERT);
    while (apic_read(LAPIC_ICRLO) & LAPIC_DLSTAT_BUSY) ;

    // Enable APIC timer for preemption.
    if (the_timesrc && !the_schedtmr) {
	static struct apic_preempt sap;
	struct apic_preempt *ap = &sap;

	apic_write(LAPIC_DCR_TIMER, LAPIC_DCRT_DIV1);
	apic_write(LAPIC_ICR_TIMER, 0xffffffff);

	apic_write(LAPIC_LVTT, APIC_TRAPNO(IRQ_TIMER));

	/* We only need this calibration to be approximate.. */
	uint64_t ccr0 = apic_read(LAPIC_CCR_TIMER);
	the_timesrc->delay_nsec(the_timesrc->arg, 10 * 1000 * 1000);
	uint64_t ccr1 = apic_read(LAPIC_CCR_TIMER);

	ap->freq_hz = (ccr0 - ccr1) * 100;
	ap->pt.arg = ap;
	ap->pt.schedule_nsec = &apic_schedule;
	the_schedtmr = &ap->pt;

	static struct interrupt_handler apic_ih = { 
	    .ih_func = &apic_intr,
	    .ih_irq  = IRQ_TIMER,
	    .ih_tbdp = BUSUNKNOWN,
	};
	irq_register(&apic_ih);

	cprintf("LAPIC: %"PRIu64" Hz\n", ap->freq_hz);
    }

    // Ack any outstanding interrupts.
    apic_write(LAPIC_EOI, 0);

    // Enable interrupts on the APIC (but not on the processor).
    apic_write(LAPIC_TPRI, 0);
}
