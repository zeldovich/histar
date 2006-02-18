#ifndef JOS_KERN_DISK_H
#define JOS_KERN_DISK_H

#include <machine/types.h>
#include <dev/pci.h>

// IDE supports at most a 64K DMA request
#define DISK_REQMAX	65536

// Support for only one data disk, at the moment

typedef enum {
    disk_io_success,
    disk_io_failure
} disk_io_status;

typedef void (*disk_callback) (disk_io_status, void *);

void disk_init(struct pci_func *pcif);
extern uint64_t disk_bytes;

typedef enum {
    op_none = 0,
    op_read,
    op_write
} disk_op;

struct iovec {
    void *iov_base;
    uint32_t iov_len;
};

int disk_io(disk_op op, struct iovec *iov_buf, int iov_cnt,
	    uint64_t offset, disk_callback cb, void *cbarg);

void ide_intr(void);

#endif
