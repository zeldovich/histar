#ifndef JOS_DEV_HPETREG_H
#define JOS_DEV_HPETREG_H

#include <dev/acpireg.h>

struct hpet_timer_reg {
    volatile uint64_t config;
    volatile uint64_t compare;
    volatile uint64_t intr;
    volatile uint64_t __pad;
};

struct hpet_reg {
    volatile uint64_t cap;
    volatile uint64_t __pad0;
    volatile uint64_t config;
    volatile uint64_t __pad1;
    volatile uint64_t isr;
    volatile uint64_t __pad2[25];
    volatile uint64_t counter;
    volatile uint64_t __pad3;
    volatile struct hpet_timer_reg timer[32];
};

struct hpet_acpi {
    struct acpi_table_hdr hdr;
    uint32_t event_block_id;
    struct acpi_gas base;
    uint8_t hpet_number;
    uint16_t min_tick;
    uint8_t page_prot;
};

#endif
