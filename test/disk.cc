#include <test/josenv.hh>
#include <test/disk.hh>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

extern "C" {
#include <kern/stackwrap.h>
}

static int disk_fd = -1;
static int disk_nbytes;

void
disk_init(int fd, int nbytes)
{
    disk_fd = fd;
    disk_nbytes = nbytes;
}

disk_io_status
stackwrap_disk_io(disk_op op, void *buf, uint32_t count, uint64_t offset)
{
    assert((count % 512) == 0);
    assert((offset % 512) == 0);
    assert(count + offset <= disk_nbytes);

    if (SAFE_EQUAL(op, op_read)) {
	assert(count == pread(disk_fd, buf, count, offset));
    } else if (SAFE_EQUAL(op, op_write)) {
	assert(count == pwrite(disk_fd, buf, count, offset));
    } else {
	printf("stackwrap_disk_io: unknown op %d\n", SAFE_UNWRAP(op));
    }

    return disk_io_success;
}
