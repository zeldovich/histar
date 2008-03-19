#ifndef JOS_KERN_DEVICE_H
#define JOS_KERN_DEVICE_H

#include <kern/kobjhdr.h>
#include <inc/device.h>

struct Device {
    struct kobject_hdr dv_ko;
    device_type dv_type;
    uint64_t dv_idx;
};

#endif
