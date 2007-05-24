#include <inc/fd.h>
#include <inc/ioctl.h>
#include <inc/stdio.h>

#include <sys/ioctl.h>
#include <errno.h>
#include <inttypes.h>

int
jos_ioctl(struct Fd *fd, uint64_t req, va_list va)
{
    struct Dev *dev;
    int r = dev_lookup(fd->fd_dev_id, &dev);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    switch (req) {
    case TCGETS:
	if (!fd->fd_isatty) {
	    __set_errno(ENOTTY);
	    return -1;
	}
    default:
	cprintf("jos_ioctl[%s]: device does not support request [0x%"PRIx64"]\n", 
		dev->dev_name, req);
	__set_errno(EPERM);
	return -1;
    }
}
