#ifndef JOS_DEV_ACPIREG_H
#define JOS_DEV_ACPIREG_H

#include <machine/types.h>

struct acpi_rsdp {
    char sig[8];
    uint8_t cksum;
    char oem[6];
    uint8_t rev;
    uint32_t rsdt_pa;
};

struct acpi_table_hdr {
    char sig[4];
    uint32_t len;
    uint8_t rev;
    uint8_t cksum;
    char oem[6];
    char oemtable[8];
    uint32_t oemrev;
    char creator[4];
    uint32_t creatorrev;
};

struct acpi_rsdt {
    struct acpi_table_hdr hdr;
    uint32_t offset[0];
};

#endif
