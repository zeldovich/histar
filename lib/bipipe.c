#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/atomic.h>
#include <inc/lib.h>
#include <inc/bipipe.h>
#include <inc/labelutil.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>

enum { bipipe_bufsz = 2000 };

struct one_pipe {
    char buf[bipipe_bufsz];

    char reader_waiting;
    char writer_waiting;
    char open;
    uint32_t read_ptr;  /* read at this offset */
    uint64_t bytes; /* # bytes in circular buffer */
    pthread_mutex_t mu;
};

struct bipipe_seg {
    struct one_pipe p[2];
};

int
bipipe(int fv[2])
{
    int r;
    struct cobj_ref seg;
    struct bipipe_seg *bs = 0;
    uint64_t ct = start_env->shared_container;
    
    uint64_t taint = handle_alloc();
    uint64_t grant = handle_alloc();
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
    bs->p[1].open = 1;
    
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
    bs->p[0].open = 1;
    
    fv[0] = fd2num(fda);
    fv[1] = fd2num(fdb);
    return 0;
}


static ssize_t
bipipe_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    struct bipipe_seg *bs = 0;
   
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
            (void **) &bs, 0);
    if (r < 0) {
        cprintf("bipipe_read: cannot segment_map: %s\n", e2s(r));
        errno = EIO;
        return -1;
    }
    struct one_pipe *op = &bs->p[fd->fd_bipipe.bipipe_a];

    size_t cc = -1;
    pthread_mutex_lock(&op->mu);
    while (op->bytes == 0) {
        int nonblock = (fd->fd_omode & O_NONBLOCK);
        op->reader_waiting = 1;
        char opn = op->open;
        pthread_mutex_unlock(&op->mu);

        if (!opn)
            return 0;

        if (nonblock) {
            errno = EAGAIN;
            goto out;
        }

        sys_sync_wait(&op->bytes, 0, sys_clock_msec() + 1000);
        pthread_mutex_lock(&op->mu);
    }

    uint32_t bufsize = sizeof(op->buf);
    uint32_t idx = op->read_ptr;

    cc = MIN(count, op->bytes);
    uint32_t cc1 = MIN(cc, bufsize-idx);        // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around
    memcpy(buf,       &op->buf[idx], cc1);
    memcpy(buf + cc1, &op->buf[0],   cc2);

    op->read_ptr = (idx + cc) % bufsize;
    op->bytes -= cc;
    if (op->writer_waiting) {
        op->writer_waiting = 0;
        sys_sync_wakeup(&op->bytes);
    }

    pthread_mutex_unlock(&op->mu);

out:
    segment_unmap_delayed(bs, 1);
    return cc;
}

static ssize_t
bipipe_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    struct bipipe_seg *bs = 0;
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
            (void **) &bs, 0);
    if (r < 0) {
        cprintf("bipipe_write: cannot segment_map: %s\n", e2s(r));
        errno = EIO;
        return -1;
    }
    struct one_pipe *op = &bs->p[!fd->fd_bipipe.bipipe_a];
    uint32_t bufsize = sizeof(op->buf);

    pthread_mutex_lock(&op->mu);
    while (op->open && op->bytes > bufsize - PIPE_BUF) {
        uint64_t b = op->bytes;
        op->writer_waiting = 1;
        pthread_mutex_unlock(&op->mu);
        sys_sync_wait(&op->bytes, b, sys_clock_msec() + 1000);
        pthread_mutex_lock(&op->mu);
    }

    if (!op->open) {
        pthread_mutex_unlock(&op->mu);
        errno = EPIPE;
        return -1;
    }

    uint32_t avail = bufsize - op->bytes;
    size_t cc = MIN(count, avail);
    uint32_t idx = (op->read_ptr + op->bytes) % bufsize;

    uint32_t cc1 = MIN(cc, bufsize - idx);      // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around

    memcpy(&op->buf[idx], buf,       cc1);
    memcpy(&op->buf[0],   buf + cc1, cc2);

    op->bytes += cc;
    if (op->reader_waiting) {
        op->reader_waiting = 0;
        sys_sync_wakeup(&op->bytes);
    }

    pthread_mutex_unlock(&op->mu);
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
    	pthread_mutex_lock(&op->mu);
        rv = op->bytes ? 1 : 0;
        pthread_mutex_unlock(&op->mu);
    } else {
    	struct one_pipe *op = &bs->p[!fd->fd_bipipe.bipipe_a];
    	pthread_mutex_lock(&op->mu);
        rv = (op->bytes > sizeof(op->buf) - PIPE_BUF) ? 0 : 1;
        pthread_mutex_unlock(&op->mu);
    }

    segment_unmap_delayed(bs, 1);
    return rv;
}

static int
bipipe_close(struct Fd *fd)
{
    struct bipipe_seg *bs = 0;
    int r = segment_map(fd->fd_bipipe.bipipe_seg, SEGMAP_READ | SEGMAP_WRITE,
            (void **) &bs, 0);
    if (r < 0) {
        cprintf("bipipe_close: cannot segment_map: %s\n", e2s(r));
        errno = EIO;
        return -1;
    }

    for (int i = 0; i < 2; i++) {
	struct one_pipe *op = &bs->p[i];
	pthread_mutex_lock(&op->mu);
	op->open = 0;
	pthread_mutex_unlock(&op->mu);
    }

    segment_unmap_delayed(bs, 1);
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
