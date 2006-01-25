#ifndef JOS_KERN_STACKWRAP_H
#define JOS_KERN_STACKWRAP_H

#include <machine/types.h>
#include <dev/disk.h>

typedef void (*stackwrap_fn) (void *);

int  stackwrap_call(stackwrap_fn fn, void *fn_arg);

disk_io_status
     stackwrap_disk_io(disk_op op, void *buf,
		       uint32_t count, uint64_t offset);

#endif
