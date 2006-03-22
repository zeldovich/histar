#include <inc/tun.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>

enum { tun_max_msg = 2000 };

struct tun_pipe {
    char buf[tun_max_msg];
    atomic64_t len;
};

struct tun_seg {
    struct tun_pipe p[2];
};

int
tun_open(struct fs_inode tino, const char *pn_suffix)
{
    struct cobj_ref tseg = tino.obj;

    int r = sys_segment_resize(tseg, sizeof(struct tun_seg));
    if (r < 0) {
	errno = EPERM;
	return -1;
    }

    int tun_a;

    if (!strcmp(pn_suffix, "tun-a")) {
	tun_a = 1;
    } else if (!strcmp(pn_suffix, "tun-b")) {
	tun_a = 0;
    } else {
	errno = EINVAL;
	return -1;
    }

    struct Fd *fd;
    r = fd_alloc(&fd, "tun");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devtun.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_tun.tun_seg = tseg;
    fd->fd_tun.tun_a = tun_a;

    return fd2num(fd);
}

static ssize_t
tun_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    struct tun_seg *ts;
    int r = segment_map(fd->fd_tun.tun_seg, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &ts, 0);
    if (r < 0) {
	cprintf("tun_read: cannot segment_map: %s\n", e2s(r));
	errno = EIO;
	return -1;
    }

    ssize_t cc = -1;
    struct tun_pipe *tp = &ts->p[fd->fd_tun.tun_a];
    uint64_t plen;

    for (;;) {
	plen = atomic_read(&tp->len);
	if (plen)
	    break;
	sys_sync_wait(&atomic_read(&tp->len), 0, ~0UL);
    }

    if (plen > len) {
	cprintf("tun_read: packet too big: %ld > %ld\n", plen, len);
	atomic_set(&tp->len, 0);
	errno = E2BIG;
	goto out;
    }

    memcpy(buf, &tp->buf[0], plen);
    atomic_set(&tp->len, 0);
    sys_sync_wakeup(&atomic_read(&tp->len));
    cc = plen;

out:
    segment_unmap_delayed(ts, 1);
    return cc;
}

static ssize_t
tun_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    if (len > tun_max_msg) {
	cprintf("tun_write: packet too big: %ld > %d\n", len, tun_max_msg);
	errno = E2BIG;
	return -1;
    }

    struct tun_seg *ts;
    int r = segment_map(fd->fd_tun.tun_seg, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &ts, 0);
    if (r < 0) {
	cprintf("tun_write: cannot segment_map: %s\n", e2s(r));
	errno = EIO;
	return -1;
    }

    struct tun_pipe *tp = &ts->p[!fd->fd_tun.tun_a];

    for (;;) {
	uint64_t plen = atomic_read(&tp->len);
	cprintf("tun_write: plen = %ld\n", plen);
	if (!plen)
	    break;
	cprintf("tun_write: blocking\n");
	sys_sync_wait(&atomic_read(&tp->len), plen, ~0UL);
    }

    memcpy(&tp->buf[0], buf, len);
    atomic_set(&tp->len, len);
    sys_sync_wakeup(&atomic_read(&tp->len));

    segment_unmap_delayed(ts, 1);
    return len;
}

static int
tun_close(struct Fd *fd)
{
    return 0;
}

struct Dev devtun = {
    .dev_id = 't',
    .dev_name = "tun",
    .dev_read = &tun_read,
    .dev_write = &tun_write,
    .dev_close = &tun_close,
};
