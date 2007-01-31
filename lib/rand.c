#include <inc/rand.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/arc4.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <stdio.h>

static struct arc4 a4;
static int a4_inited;

int
rand_open(int flags)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "rand");
    if (r < 0) {
    	errno = ENOMEM;
    	return -1;
    }

    fd->fd_dev_id = devrand.dev_id;
    fd->fd_omode = flags;

    return fd2num(fd);
}

static void
rand_init(void)
{
    uint64_t keybuf[4];
    keybuf[0] = thread_id();
    keybuf[1] = sys_clock_msec();
    keybuf[2] = sys_pstate_timestamp();
    keybuf[3] = sys_pstate_timestamp();

    arc4_reset(&a4);
    arc4_setkey(&a4, &keybuf[0], sizeof(keybuf));
}

static ssize_t
rand_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    if (!a4_inited) {
	rand_init();
	a4_inited = 1;
    }

    char *cbuf = (char *) buf;
    for (uint64_t i = 0; i < len; i++)
	cbuf[i] = arc4_getbyte(&a4);
    return len;
}

static ssize_t
rand_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    return 0;
}

static int
rand_probe(struct Fd *fd, dev_probe_t probe)
{
    return 1;
}

static int
rand_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{
    return -1;
}

static int
rand_close(struct Fd *fd)
{
    return 0;
}

struct Dev devrand = {
    .dev_id = 'r',
    .dev_name = "rand",
    .dev_read = &rand_read,
    .dev_write = &rand_write,
    .dev_probe = &rand_probe,
    .dev_statsync = &rand_statsync,
    .dev_close = &rand_close,
};
