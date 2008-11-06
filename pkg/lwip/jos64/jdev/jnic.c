#include <inc/array.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/udev.h>

#include <jdev/jnic.h>
#include <jdev/knic.h>
#include <jdev/ne2kpci.h>
#include <jdev/e1000.h>

#include <string.h>
#include <malloc.h>

struct {
    uint64_t key;
    struct jnic_device *dev;
} devices[] = {
    { KEYKERNEL, &knic_jnic },
    { MK_PCIKEY(device_net, 0x10ec, 0x8029), &ne2kpci_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x100e), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x100f), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x107c), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x108c), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x109a), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x1079), &e1000_jnic },
    { MK_PCIKEY(device_net, 0x8086, 0x105e), &e1000_jnic },
};

int
jnic_net_macaddr(struct jnic* jnic, uint8_t* macaddr)
{
    return devices[jnic->idx].dev->net_macaddr(jnic->arg, macaddr);
}

int
jnic_net_buf(struct jnic* jnic, struct cobj_ref seg,
	     uint64_t offset, netbuf_type type)
{
    return devices[jnic->idx].dev->net_buf(jnic->arg, seg, offset, type);    
}

int64_t
jnic_net_wait(struct jnic* jnic, uint64_t waiter_id, int64_t waitgen)
{
    return devices[jnic->idx].dev->net_wait(jnic->arg, waiter_id, waitgen);        
}

int
jnic_match(struct jnic* jnic, struct cobj_ref obj, uint64_t key)
{
    int r = 0;

    for (uint32_t i = 0; i < array_size(devices); i++) {
	if (devices[i].key == key) {
	    if (jnic && !(r = devices[i].dev->init(obj, &jnic->arg)))
		jnic->idx = i;
	    return r;
	}
    }
    return -E_INVAL;
}
