#include <kern/lib.h>
#include <dev/disk.h>
#include <inc/error.h>

int
disk_io(disk_op op, struct kiovec *iov_buf, int iov_cnt,
        uint64_t byte_offset, disk_callback cb, void *cbarg)
{
    cprintf("XXX disk_io unimpl\n");
    return -E_INVAL;
}

void
disk_poll(void)
{
    cprintf("XXX disk_poll unimpl\n");
}
