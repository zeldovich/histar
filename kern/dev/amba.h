#ifndef JOS_DEV_AMBA_H
#define JOS_DEV_AMBA_H

#include <machine/types.h>

void amba_init(void);
void amba_print(void);

/* AHB slaves */
struct amba_ahb_device {
    uint32_t start[4];
    uint32_t stop[4];
    uint32_t irq;
};

uint32_t amba_ahbslv_device(uint32_t vendor, 
			    uint32_t device, 
			    struct amba_ahb_device *dev,
			    uint32_t nr);

/* APB slaves */
struct amba_apb_device {
    uint32_t start;
    uint32_t irq;
};

uint32_t amba_apbslv_device(uint32_t vendor, 
			    uint32_t device, 
			    struct amba_apb_device * dev, 
			    uint32_t nr);

#endif
