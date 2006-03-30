#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>
#include <inc/bipipe.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>

enum { bipipe_max_msg = 2000 };

struct one_pipe {
    char buf[bipipe_max_msg];
    atomic64_t len;
};

struct bipipe_seg {
    struct one_pipe p[2];
    int open[2];
};

int
bipipe(int fv[2])
{
    int r;
    struct cobj_ref seg;
    struct bipipe_seg *bs = 0;
    uint64_t ct = start_env->shared_container;
    
    uint64_t taint = sys_handle_create();
    uint64_t grant = sys_handle_create();
    struct ulabel *l = label_alloc();
    l->ul_default = 1;
    label_set_level(l, taint, 3, 1);
    label_set_level(l, grant, 0, 1);
    if ((r = segment_alloc(ct, sizeof(*bs), &seg, (void *)&bs, l, "bipipe")) < 0) {
        errno = ENOMEM;
        return -1;        
    }

    memset(bs, 0, sizeof(*bs));

    struct Fd *fda;
    r = fd_alloc(&fda, "bipipe");
    if (r < 0) {
        segment_unmap_delayed(bs, 1);
        errno = ENOMEM;
        return -1;
    }
    fda->fd_dev_id = devbipipe.dev_id;
    fda->fd_omode = O_RDWR;
    fda->fd_bipipe.bipipe_seg = seg;
    fda->fd_bipipe.bipipe_a = 1;
    bs->open[1] = 1;
    
    struct Fd *fdb;
    r = fd_alloc(&fdb, "bipipe");
    if (r < 0) {
        fd_close(fda);
        errno = ENOMEM;
        return -1;
    }
    fdb->fd_dev_id = devbipipe.dev_id;
    fdb->fd_omode = O_RDWR;
    fdb->fd_bipipe.bipipe_seg = seg;
    fdb->fd_bipipe.bipipe_a = 0;   
    bs->open[0] = 1;
    
    fv[0] = fd2num(fda);
    fv[1] = fd2num(fdb);
    return 0;
}

static ssize_t
bipipe_read(struct Fd *fd, void *buf, size_t len, off_t offset)
{
    struct bipipe_seg *bs = 0;
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
            (void **) &bs, 0);
    if (r < 0) {
        cprintf("bipipe_read: cannot segment_map: %s\n", e2s(r));
        errno = EIO;
        return -1;
    }

    ssize_t cc = -1;
    struct one_pipe *op = &bs->p[fd->fd_bipipe.bipipe_a];
    uint64_t plen;

    for (;;) {
        plen = atomic_read(&op->len);
        if (plen)
            break;
    
        if ((fd->fd_omode & O_NONBLOCK)) {
            errno = EAGAIN;
            goto out;
        }
        sys_sync_wait(&atomic_read(&op->len), 0, ~0UL);
    }

    if (plen > len) {
        cprintf("bipipe_read: too big for buf: %ld > %ld\n", plen, len);
        atomic_set(&op->len, 0);
        errno = E2BIG;
        goto out;
    }

    memcpy(buf, &op->buf[0], plen);
    atomic_set(&op->len, 0);
    sys_sync_wakeup(&atomic_read(&op->len));
    cc = plen;

out:
    segment_unmap_delayed(bs, 1);
    return cc;
}


static ssize_t
bipipe_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    if (len > bipipe_max_msg) {
    	cprintf("bipipe_write: buf too big: %ld > %d\n", len, bipipe_max_msg);
    	errno = E2BIG;
    	return -1;
    }

    struct bipipe_seg *bs = 0;
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &bs, 0);
    if (r < 0) {
    	cprintf("bipipe_write: cannot segment_map: %s\n", e2s(r));
    	errno = EIO;
    	return -1;
    }

    ssize_t cc = -1;
    struct one_pipe *op = &bs->p[!fd->fd_bipipe.bipipe_a];

    for (;;) {
    	uint64_t plen = atomic_read(&op->len);

    	if (!plen)
    	    break;
    
    	if ((fd->fd_omode & O_NONBLOCK)) {
    	    errno = EAGAIN;
    	    goto out;
    	}
    
    	sys_sync_wait(&atomic_read(&op->len), plen, ~0UL);
    }
    memcpy(&op->buf[0], buf, len);
    atomic_set(&op->len, len);
    sys_sync_wakeup(&atomic_read(&op->len));
    cc = len;

out:
    segment_unmap_delayed(bs, 1);
    return cc;
}

static int
bipipe_probe(struct Fd *fd, dev_probe_t probe)
{
    struct bipipe_seg *bs = 0;
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
			 (void **) &bs, 0);
    if (r < 0) {
    	cprintf("bipipe_probe: cannot segment_map: %s\n", e2s(r));
    	errno = EIO;
    	return -1;
    }

    int rv;
    if (probe == dev_probe_read) {
    	struct one_pipe *op = &bs->p[fd->fd_bipipe.bipipe_a];
    	rv = atomic_read(&op->len) ? 1 : 0;
    } else {
    	struct one_pipe *op = &bs->p[!fd->fd_bipipe.bipipe_a];
    	rv = atomic_read(&op->len) ? 0 : 1;
    }

    segment_unmap_delayed(bs, 1);
    return rv;
}

static int
bipipe_close(struct Fd *fd)
{
    return 0;
}
           
struct Dev devbipipe = {
    .dev_id = 'b',
    .dev_name = "bipipe",
    .dev_read = &bipipe_read,
    .dev_write = &bipipe_write,
    .dev_probe = &bipipe_probe,
    .dev_close = &bipipe_close,
};
