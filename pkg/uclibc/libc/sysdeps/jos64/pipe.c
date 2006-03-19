#include <inc/fd.h>
#include <inc/lib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

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

    int fdnum = fd2num(fd);
    int ofd = dup(fdnum);
    if (ofd < 0) {
	fd_close(fd);
	return -1;
    }

    fds[0] = fdnum;
    fds[1] = ofd;
    return 0;
}

static ssize_t
pipe_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    if (atomic_read(&fd->fd_ref) == 1) {
	raise(SIGPIPE);
	__set_errno(EPIPE);
	return -1;
    }

    uint32_t bufsize = sizeof(fd->fd_pipe.buf);

    pthread_mutex_lock(&fd->fd_pipe.mu);
    while (fd->fd_pipe.bytes > bufsize - PIPE_BUF) {
	uint64_t b = fd->fd_pipe.bytes;
	pthread_mutex_unlock(&fd->fd_pipe.mu);
	sys_sync_wait(&fd->fd_pipe.bytes, b, ~0UL);
	pthread_mutex_lock(&fd->fd_pipe.mu);
    }

    uint32_t avail = bufsize - fd->fd_pipe.bytes;
    size_t cc = MIN(count, avail);
    uint32_t idx = (fd->fd_pipe.read_ptr + fd->fd_pipe.bytes) % bufsize;

    uint32_t cc1 = MIN(cc, bufsize - idx);	    // idx to end-of-buffer
    uint32_t cc2 = (cc1 == cc) ? 0 : (cc - cc1);    // wrap-around

    memcpy(&fd->fd_pipe.buf[idx], buf,       cc1);
    memcpy(&fd->fd_pipe.buf[0],   buf + cc1, cc2);

    fd->fd_pipe.bytes += cc;
    sys_sync_wakeup(&fd->fd_pipe.bytes);

    pthread_mutex_unlock(&fd->fd_pipe.mu);
    return count;
}

static ssize_t
pipe_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    pthread_mutex_lock(&fd->fd_pipe.mu);
    while (fd->fd_pipe.bytes == 0) {
	uint32_t ref = atomic_read(&fd->fd_ref);
	int nonblock = (fd->fd_omode & O_NONBLOCK);
	pthread_mutex_unlock(&fd->fd_pipe.mu);

    	// EOF when the other end has been closed
    	if (ref == 1)
    	    return 0;

	if (nonblock) {
	    __set_errno(EAGAIN);
	    return -1;
	}

    	// Need to periodically wake up and check for EOF
	sys_sync_wait(&fd->fd_pipe.bytes, 0, sys_clock_msec() + 1000);
    	pthread_mutex_lock(&fd->fd_pipe.mu);
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
    sys_sync_wakeup(&fd->fd_pipe.bytes);

    pthread_mutex_unlock(&fd->fd_pipe.mu);
    return cc;
}

static int
pipe_close(struct Fd *fd)
{
    // Wake up any readers that might be waiting for EOF.
    // Not completely reliable; we still need to check for EOF
    // with a timeout in pipe_read().
    sys_sync_wakeup(&fd->fd_pipe.bytes);
}

static int
pipe_probe(struct Fd *fd, dev_probe_t probe)
{
    if (probe == dev_probe_write) {
        if (fd->fd_pipe.bytes >  sizeof(fd->fd_pipe.buf) - PIPE_BUF)
            return 0 ;
        return 1 ;   
    }
    return fd->fd_pipe.bytes ;
}

static int
pipe_stat(struct Fd *fd, struct stat *buf)
{
    memset(buf, 0, sizeof(*buf));
    return 0;
}

struct Dev devpipe = {
    .dev_id = 'p',
    .dev_name = "pipe",
    .dev_read = pipe_read,
    .dev_write = pipe_write,
    .dev_close = pipe_close,
    .dev_probe = pipe_probe,
    .dev_stat = pipe_stat,
};
