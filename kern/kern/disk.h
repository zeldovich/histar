#ifndef JOS_KERN_DISK_H
#define JOS_KERN_DISK_H

#include <machine/types.h>
#include <dev/pci.h>
#include <inc/safetype.h>
#include <inc/queue.h>

// IDE supports at most a 64K DMA request
#define DISK_REQMAX	65536

typedef SAFE_TYPE(int) disk_io_status;
#define disk_io_success SAFE_WRAP(disk_io_status, 1)
#define disk_io_failure SAFE_WRAP(disk_io_status, 2)

typedef void (*disk_callback) (disk_io_status, void *);

typedef SAFE_TYPE(int) disk_op;
#define op_none  SAFE_WRAP(disk_op, 0)
#define op_read  SAFE_WRAP(disk_op, 1)
#define op_write SAFE_WRAP(disk_op, 2)
#define op_flush SAFE_WRAP(disk_op, 3)

struct kiovec {
    void *iov_base;
    uint32_t iov_len;
};

struct disk {
    uint64_t dk_bytes;
    char dk_model[40];
    char dk_serial[20];
    char dk_firmware[8];
    char dk_busloc[20];

    /* Driver hooks */
    int  (*dk_issue)(struct disk*, disk_op, struct kiovec*, int,
		     uint64_t, disk_callback, void*);
    void (*dk_poll)(struct disk*);
    void *dk_arg;

    LIST_ENTRY(disk) dk_link;
};

void disk_poll(struct disk *dk);
int  disk_io(struct disk *dk, disk_op op, struct kiovec *iov_buf, int iov_cnt,
	     uint64_t offset, disk_callback cb, void *cbarg)
	__attribute__ ((warn_unused_result));

extern LIST_HEAD(disk_list, disk) disks;

#endif
