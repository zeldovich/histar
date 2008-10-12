#ifndef JOS_DEV_IOAPIC_H
#define JOS_DEV_IOAPIC_H

#include <machine/mp.h>

void ioapic_init(struct apic* apic);
void ioapic_rdtr(struct apic* apic, uint32_t sel, uint32_t* hi, uint32_t* lo);
void ioapic_rdtw(struct apic* apic, uint32_t sel, uint32_t hi, uint32_t lo);

#endif
