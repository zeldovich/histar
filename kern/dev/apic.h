#ifndef JOS_DEV_APIC_H
#define JOS_DEV_APIC_H

#define APIC_SPURIOUS	0xff

void apic_init(void);
void apic_send_ipi(uint32_t target, uint32_t vector);

#endif
