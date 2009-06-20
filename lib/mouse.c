#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <inttypes.h>

static int
mouse_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "mouse");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devmouse.dev_id;
    fd->fd_omode = flags;
    fd->fd_file.ino = ino;
    return fd2num(fd);
}

static int
mouse_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    __set_errno(ENOSYS);
    return -1;
}

static ssize_t
mouse_read(struct Fd *fd, void *buf, size_t nbytes, off_t off)
{
    int64_t r = sys_obj_read(fd->fd_file.ino.obj, buf, nbytes, off);
    return (ssize_t)r;
}

static ssize_t
mouse_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    return len;
}

static int
mouse_close(struct Fd *fd)
{
    return 0;
}

static int
mouse_probe(struct Fd *fd, dev_probe_t probe)
{
    if (probe == dev_probe_read) {
	// TODO Why does this fail?
	//int p = sys_obj_probe(fd->fd_file.ino.obj);
	//cprintf("(%d)", p);
	//return p;
	// need to figure this out or Xorg pry ends up polling
	return 1;
    }

    return 0;
}

static int
mouse_statsync(struct Fd *fd, dev_probe_t probe,
               struct wait_stat *wstat, int wslot_avail)
{
    // TODO I have no idea what the semantics of this function are
    return 1;
}

struct Dev devmouse = {
    .dev_id = 'm',
    .dev_name = "mouse",
    .dev_open = mouse_open,
    .dev_ioctl = mouse_ioctl,
    .dev_read = mouse_read,
    .dev_write = mouse_write,
    .dev_close = mouse_close,
    .dev_probe = mouse_probe,
    .dev_statsync = mouse_statsync,
};

