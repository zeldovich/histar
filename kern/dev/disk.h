#ifndef JOS_KERN_DISK_H
#define JOS_KERN_DISK_H

#include <machine/types.h>
#include <dev/pci.h>
#include <inc/safetype.h>

// IDE supports at most a 64K DMA request
#define DISK_REQMAX	65536

// Support for only one data disk, at the moment

typedef SAFE_TYPE(int) disk_io_status;
#define disk_io_success SAFE_WRAP(disk_io_status, 1)
#define disk_io_failure SAFE_WRAP(disk_io_status, 2)

typedef void (*disk_callback) (disk_io_status, void *);

void disk_init(struct pci_func *pcif);
extern uint64_t disk_bytes;

typedef SAFE_TYPE(int) disk_op;
#define op_none  SAFE_WRAP(disk_op, 0)
#define op_read  SAFE_WRAP(disk_op, 1)
#define op_write SAFE_WRAP(disk_op, 2)
#define op_flush SAFE_WRAP(disk_op, 3)

struct kiovec {
    void *iov_base;
    uint32_t iov_len;
};

int disk_io(disk_op op, struct kiovec *iov_buf, int iov_cnt,
	    uint64_t offset, disk_callback cb, void *cbarg)
	__attribute__ ((warn_unused_result));

void disk_poll(void);

#endif
