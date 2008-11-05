#ifndef JOS_KERN_DEVICE_H
#define JOS_KERN_DEVICE_H

#include <kern/kobjhdr.h>
#include <kern/as.h>
#include <inc/device.h>

struct Device {
    struct kobject_hdr dv_ko;
    uint8_t  dv_type;
    uint64_t dv_idx;
    const struct Address_space *dv_as;

    struct segmap_list dv_segmap_list;
};

void	device_swapin(struct Device *dv);
int	device_set_as(const struct Device *const_dv, struct cobj_ref asref)
	__attribute__((warn_unused_result));

#endif
