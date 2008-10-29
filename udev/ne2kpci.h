#ifndef JOS_UDEV_NE2KPCI_H
#define JOS_UDEV_NE2KPCI_H

#include <inc/netdev.h>
#include <inc/container.h>

int ne2kpci_init(struct cobj_ref obj, void** arg);
int ne2kpci_macaddr(void *arg, uint8_t* macaddr);
int ne2kpci_buf(void *arg, struct cobj_ref seg,
		uint64_t offset, netbuf_type type);
int64_t ne2kpci_wait(void *arg, uint64_t waiter_id, int64_t waitgen);

#endif
