#include <machine/pmap.h>
#include <dev/acpi.h>
#include <dev/hpet.h>
#include <kern/arch.h>
#include <kern/lib.h>

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
 * Initialization code.
 */
static void
acpi_load_table(struct acpi_table_hdr *th)
{
    cprintf("ACPI table: %.4s\n", th->sig);

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

    //cprintf("ACPI: %.6s at %p\n", rsdp->oem, rsdp);
    struct acpi_rsdt *rsdt = pa2kva(rsdp->rsdt_pa);
    uint32_t nent = (rsdt->hdr.len - sizeof(rsdt->hdr)) /
		    sizeof(rsdt->offset[0]);
    for (uint32_t i = 0; i < nent; i++)
	acpi_load_table(pa2kva(rsdt->offset[i]));
}
