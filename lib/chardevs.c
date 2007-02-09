#include <inc/chardevs.h>
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/ioctl.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>

int
jos_devnull_open(int flags)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "devnull");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devnull.dev_id;
    fd->fd_omode = flags;
    return fd2num(fd);
}

int
jos_devzero_open(int flags)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "devzero");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devzero.dev_id;
    fd->fd_omode = flags;
    return fd2num(fd);
}

static ssize_t
zero_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    memset(buf, 0, len);
    return len;
}

static ssize_t
null_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    return 0;
}

static ssize_t
null_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    return len;
}

static int
null_probe(struct Fd *fd, dev_probe_t probe)
{
    return 1;
}

static int
null_close(struct Fd *fd)
{
    return 0;
}

struct Dev devnull = {
    .dev_id = 'n',
    .dev_name = "null",
    .dev_read = &null_read,
    .dev_write = &null_write,
    .dev_probe = &null_probe,
    .dev_close = &null_close,
    .dev_ioctl = &jos_ioctl,
};

struct Dev devzero = {
    .dev_id = 'z',
    .dev_name = "zero",
    .dev_read = &zero_read,
    .dev_write = &null_write,
    .dev_probe = &null_probe,
    .dev_close = &null_close,
    .dev_ioctl = &jos_ioctl,
};
