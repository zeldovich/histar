#include <kern/disk.h>

struct disk_list disks;

void
disk_poll(struct disk *dk)
{
    dk->dk_poll(dk);
}

int
disk_io(struct disk *dk, disk_op op, struct kiovec *iov_buf, int iov_cnt,
	uint64_t offset, disk_callback cb, void *cbarg)
{
    return dk->dk_issue(dk, op, iov_buf, iov_cnt, offset, cb, cbarg);
}
