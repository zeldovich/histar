#ifndef JOS_DEV_AMBA_H
#define JOS_DEV_AMBA_H

#include <machine/types.h>

struct amba_apb_device {
    uint32_t start;
    uint32_t irq;
    uint32_t bus_id;
};

void amba_init(void);
uint32_t amba_find_apbslv_addr(uint32_t vendor, uint32_t device, uint32_t *irq);

uint32_t amba_find_next_apbslv_devices(uint32_t vendor, 
				       uint32_t device, 
				       struct amba_apb_device * dev, 
				       uint32_t nr);

#endif
