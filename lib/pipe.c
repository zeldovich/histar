#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/ioctl.h>
#include <inc/multisync.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/stat.h>

int
pipe(int fds[2])
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "pipe fd");
    if (r < 0) {
	__set_errno(ENOMEM);
	return -1;
    }

    fd->fd_dev_id = devpipe.dev_id;
    fd->fd_omode = O_RDWR;

    fd->fd_pipe.read_ptr = 0;
    fd->fd_pipe.bytes = 0;
    memset(&fd->fd_pipe.mu, 0, sizeof(fd->fd_pipe.mu));
    fd->fd_pipe.reader_waiting = 0;
    fd->fd_pipe.writer_waiting = 0;

    int fdnum = fd2num(fd);
    int ofd = dup(fdnum);
    if (ofd < 0) {
	jos_fd_close(fd);
	return -1;
    }

    fds[0] = fdnum;
    fds[1] = ofd;
    return 0;
}

static ssize_t
pipe_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    if (jos_atomic_read(&fd->fd_ref) == 1) {
	raise(SIGPIPE);
	__set_errno(EPIPE);
	return -1;
    }

    uint32_t bufsize = sizeof(fd->fd_pipe.buf);

    jthread_mutex_lock(&fd->fd_pipe.mu);
    while (fd->fd_pipe.bytes > bufsize - PIPE_BUF) {
	uint64_t b = fd->fd_pipe.bytes;
	fd->fd_pipe.writer_waiting = 1;
	jthread_mutex_unlock(&fd->fd_pipe.mu);
	sys_sync_wait(&fd->fd_pipe.bytes, b, UINT64(~0));
	jthread_mutex_lock(&fd->fd_pipe.mu);
    }

    uint32_t avail = bufsize - fd->fd_pipe.bytes;
    size_t cc = MIN(count, avail);
    uint32_t idx = (fd->fd_pipe.read_ptr + fd->fd_pipe.bytes) % bufsize;

    uint32_t cc1 = MIN(cc, bufsize - idx);	    // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around

    memcpy(&fd->fd_pipe.buf[idx], buf,       cc1);
    memcpy(&fd->fd_pipe.buf[0],   buf + cc1, cc2);

    fd->fd_pipe.bytes += cc;
    if (fd->fd_pipe.reader_waiting) {
	fd->fd_pipe.reader_waiting = 0;
	sys_sync_wakeup(&fd->fd_pipe.bytes);
    }

    jthread_mutex_unlock(&fd->fd_pipe.mu);
    return cc;
}

static ssize_t
pipe_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    jthread_mutex_lock(&fd->fd_pipe.mu);
    while (fd->fd_pipe.bytes == 0) {
	uint32_t ref = jos_atomic_read(&fd->fd_ref);
	int nonblock = (fd->fd_omode & O_NONBLOCK);
	fd->fd_pipe.reader_waiting = 1;
	jthread_mutex_unlock(&fd->fd_pipe.mu);

    	// EOF when the other end has been closed
    	if (ref == 1)
    	    return 0;

	if (nonblock) {
	    __set_errno(EAGAIN);
	    return -1;
	}
	
	struct wait_stat wstat[2];
	memset(wstat, 0, sizeof(wstat));
	WS_SETADDR(&wstat[0], &fd->fd_pipe.bytes);
	WS_SETVAL(&wstat[0], 0);
	WS_SETADDR(&wstat[1], &fd->fd_ref64);
	WS_SETVAL(&wstat[1], ref);
	if (multisync_wait(wstat, 2, UINT64(~0)) < 0)
	    return -1;

    	jthread_mutex_lock(&fd->fd_pipe.mu);
    }

    uint32_t bufsize = sizeof(fd->fd_pipe.buf);
    uint32_t idx = fd->fd_pipe.read_ptr;

    size_t cc = MIN(count, fd->fd_pipe.bytes);
    uint32_t cc1 = MIN(cc, bufsize-idx);	    // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around
    memcpy(buf,       &fd->fd_pipe.buf[idx], cc1);
    memcpy(buf + cc1, &fd->fd_pipe.buf[0],   cc2);

    fd->fd_pipe.read_ptr = (idx + cc) % bufsize;
    fd->fd_pipe.bytes -= cc;
    if (fd->fd_pipe.writer_waiting) {
	fd->fd_pipe.writer_waiting = 0;
	sys_sync_wakeup(&fd->fd_pipe.bytes);
    }

    jthread_mutex_unlock(&fd->fd_pipe.mu);
    return cc;
}

static int
pipe_close(struct Fd *fd)
{
    return 0;
}

static int
pipe_probe(struct Fd *fd, dev_probe_t probe)
{
    if (probe == dev_probe_write) {
	if (fd->fd_pipe.bytes >  sizeof(fd->fd_pipe.buf) - PIPE_BUF)
	    return 0;
	return 1;
    }

    if (fd->fd_pipe.bytes)
	return 1;

    if (jos_atomic_read(&fd->fd_ref) == 1)
	return 1;

    return 0;
}

static int
pipe_statsync_cb0(struct wait_stat *ws, dev_probe_t probe,
		  volatile uint64_t **addrp, void **arg1)
{
    struct Fd *fd = (struct Fd *) ws->ws_cbarg;
    
    if (probe == dev_probe_write)
	fd->fd_pipe.writer_waiting = 1;
    else
	fd->fd_pipe.reader_waiting = 1;
    return 0;
}

static int
pipe_statsync(struct Fd *fd, dev_probe_t probe,
	      struct wait_stat *wstat, int wslot_avail)
{
    if (wslot_avail < 2)
	return -1;

    memset(wstat, 0, sizeof(*wstat) * 2);

    WS_SETADDR(wstat, &fd->fd_pipe.bytes);
    WS_SETVAL(wstat, fd->fd_pipe.bytes);
    WS_SETADDR(wstat + 1, &fd->fd_ref64);
    WS_SETVAL(wstat + 1, fd->fd_ref64);

    WS_SETCB0(wstat, &pipe_statsync_cb0);
    wstat->ws_cbarg = fd;
    wstat->ws_probe = probe;
    return 2;
}

static int
pipe_getsockopt(struct Fd *fd, int level, int optname,
		void *optval, socklen_t *optlen)
{
    errno = ENOTSOCK;
    return -1;
}

struct Dev devpipe = {
    .dev_id = 'p',
    .dev_name = "pipe",
    .dev_read = pipe_read,
    .dev_write = pipe_write,
    .dev_close = pipe_close,
    .dev_probe = pipe_probe,
    .dev_statsync = pipe_statsync,
    .dev_getsockopt = pipe_getsockopt,
    .dev_ioctl = jos_ioctl,
};
