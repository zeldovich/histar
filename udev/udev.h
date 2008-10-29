#ifndef JOS_UDEV_UDEV_H
#define JOS_UDEV_UDEV_H

#define MK_PCIKEY(type, vendor, device) \
	((uint64_t)(type) << 56) | ((uint64_t)(device) << 16) | (vendor)

#define KEYKERNEL UINT64(~0)

#define KEYTYPE(key) ((key) >> 56)

#endif
