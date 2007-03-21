#include <machine/pmap.h>
#include <dev/acpi.h>
#include <kern/arch.h>
#include <inc/error.h>

static int
acpi_rsdp_scan(physaddr_t start, uint64_t len, struct acpi_rsdp **basep)
{
    for (physaddr_t pa = start; pa < start + len; pa += 16) {
	struct acpi_rsdp *rsdp = pa2kva(pa);
	if (memcmp(&rsdp->sig[0], "RSD PTR ", 8))
	    continue;
	if (acpi_checksum(rsdp, sizeof(*rsdp))) {
	    cprintf("acpi_rsdp_scan: bad checksum at %p\n", rsdp);
	    continue;
	}

	*basep = rsdp;
	return 0;
    }

    return -E_NOT_FOUND;
}

/*
 * See ACPI spec, 5.2.5.1 Finding the RSDP on IA-PC Systems
 */
int
acpi_rsdp_find(struct acpi_rsdp **basep)
{
    uint16_t *ebdap = pa2kva(0x40e);
    physaddr_t ebda = *ebdap;

    if (acpi_rsdp_scan(ebda << 4, 1024, basep) == 0)
	return 0;
    if (acpi_rsdp_scan(0xe0000, 0x20000, basep) == 0)
	return 0;

    return -E_NOT_FOUND;
}
