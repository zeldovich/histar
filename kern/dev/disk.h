#ifndef JOS_KERN_DISK_H
#define JOS_KERN_DISK_H

#include <machine/types.h>

// Support for only one data disk, at the moment

typedef enum {
    disk_io_success,
    disk_io_failure
} disk_io_status;

typedef void (*disk_callback) (disk_io_status, void *, uint32_t, uint64_t, void *);

void disk_init();
extern uint64_t disk_bytes;

typedef enum {
    op_none = 0,
    op_read,
    op_write
} disk_op;

int disk_io(disk_op op, void *buf,
	    uint32_t count, uint64_t offset,
	    disk_callback cb, void *cbarg);

void disk_test();

#endif
