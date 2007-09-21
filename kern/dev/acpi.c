#include <machine/pmap.h>
#include <machine/x86.h>
#include <dev/acpi.h>
#include <dev/hpet.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <kern/timer.h>

enum { acpi_debug = 0 };

/*
 * Helper functions.
 */
uint8_t
acpi_checksum(const void *base, uint64_t len)
{
    const uint8_t *p = base;
    uint8_t sum = 0;

    for (uint64_t i = 0; i < len; i++)
	sum += p[i];

    return sum;
}

/*
 * ACPI PM Timer: ACPI spec, section 4.7.3.3.
 */
struct acpi_pmtimer {
    struct time_source timesrc;

    uint32_t ioaddr;
    uint32_t mask;

    uint32_t last_read;
    uint64_t ticks;
};

static uint64_t
acpi_pmtimer_ticks(void *arg)
{
    struct acpi_pmtimer *pmt = (struct acpi_pmtimer *) arg;

    /*
     * Compensate for the limited counter width by keeping our own count.
     * Requires that we read the ticks often enough to avoid overflow.
     * With a 24-bit counter that means at least once every ~4 seconds.
     */
    uint32_t val = inl(pmt->ioaddr);
    uint32_t diff = (val - pmt->last_read) & pmt->mask;
    pmt->last_read = val;
    pmt->ticks += diff;

    return pmt->ticks;
}

static void
acpi_pmtimer_delay(void *arg, uint64_t nsec)
{
    struct acpi_pmtimer *pmt = (struct acpi_pmtimer *) arg;
    uint64_t ticks = timer_convert(nsec, pmt->timesrc.freq_hz, 1000000000);

    uint64_t start = acpi_pmtimer_ticks(pmt);
    while (acpi_pmtimer_ticks(pmt) < start + ticks)
	;
}

static void
acpi_pmtimer_init(uint32_t ioaddr, int extflag)
{
    if (the_timesrc)
	return;

    static struct acpi_pmtimer the_pmt;
    struct acpi_pmtimer *pmt = &the_pmt;

    pmt->timesrc.type = time_source_pmt;
    pmt->timesrc.freq_hz = 3579545;
    pmt->timesrc.arg = pmt;
    pmt->timesrc.ticks = &acpi_pmtimer_ticks;
    pmt->timesrc.delay_nsec = &acpi_pmtimer_delay;

    pmt->ioaddr = ioaddr;
    pmt->mask = extflag ? 0xffffffff : 0xffffff;
    pmt->last_read = inl(ioaddr);
    pmt->ticks = 0;

    cprintf("ACPI: %d-bit PM timer at 0x%x\n", extflag ? 32 : 24, ioaddr);
    the_timesrc = &pmt->timesrc;
}

/*
 * Initialization code.
 */
static void
acpi_load_fadt(struct acpi_table_hdr *th)
{
    struct acpi_fadt *fadt = (struct acpi_fadt *) th;
    acpi_pmtimer_init(fadt->pm_tmr_blk, fadt->flags & FADT_TMR_VAL_EXT);
}

static void
acpi_load_table(struct acpi_table_hdr *th)
{
    if (acpi_debug)
	cprintf("ACPI table: %.4s\n", th->sig);

    if (!memcmp(th->sig, "FACP", 4))
	acpi_load_fadt(th);
    if (!memcmp(th->sig, "HPET", 4))
	hpet_attach(th);
}

void
acpi_init(void)
{
    struct acpi_rsdp *rsdp;
    int r = acpi_rsdp_find(&rsdp);
    if (r < 0) {
	cprintf("ACPI: not found\n");
	return;
    }

    if (acpi_debug)
	cprintf("ACPI: %.6s at %p\n", rsdp->oem, rsdp);

    struct acpi_rsdt *rsdt = pa2kva(rsdp->rsdt_pa);
    uint32_t nent = (rsdt->hdr.len - sizeof(rsdt->hdr)) /
		    sizeof(rsdt->offset[0]);
    for (uint32_t i = 0; i < nent; i++)
	acpi_load_table(pa2kva(rsdt->offset[i]));
}
