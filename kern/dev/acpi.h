#ifndef JOS_DEV_ACPI_H
#define JOS_DEV_ACPI_H

#include <dev/acpireg.h>

/* Boot-time initialization */
void acpi_init(void);

/* Helper functions */
uint8_t acpi_checksum(const void *base, uint64_t len);

/* Machine-specific */
int acpi_rsdp_find(struct acpi_rsdp **basep);

#endif
