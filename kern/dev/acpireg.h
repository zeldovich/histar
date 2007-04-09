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

struct acpi_gas {
    uint8_t as_id;
    uint8_t reg_bit_width;
    uint8_t reg_bit_offset;
    uint8_t access_size;
    uint64_t addr;
} __attribute__((__packed__));

struct acpi_rsdt {
    struct acpi_table_hdr hdr;
    uint32_t offset[0];
};

struct acpi_fadt {
    struct acpi_table_hdr hdr;
    uint32_t facs_pa;
    uint32_t dsdt_pa;		// offset 40
    uint32_t __pad0[8];
    uint32_t pm_tmr_blk;	// offset 76
    uint32_t __pad1[8];
    uint32_t flags;		// offset 112
    /* We don't care about the rest, for now */
};

#define FADT_TMR_VAL_EXT	(1 << 8)

#endif
