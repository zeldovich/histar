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
#include <sys/socket.h>

#include <inc/remfile.h>

int remfile_open(char *host, int port, char *path);

int
remfile_open(char *host, int port, char *path)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "remfile");
    if (r < 0) {
        errno = ENOMEM;
        return -1;
    }
    fd->fd_dev_id = devremfile.dev_id;
    fd->fd_omode = O_RDWR;

    if ((r = remfiled_open(host, port, path, &fd->fd_remfile.ino)) < 0) {
        jos_fd_close(fd);
        errno = r;
        return -1;    
    }
    
    return fd2num(fd);
}


static ssize_t
remfile_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return remfiled_read(fd->fd_remfile.ino, buf, count, offset);
}

static ssize_t
remfile_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return remfiled_write(fd->fd_remfile.ino, buf, count, offset);
}

static int
remfile_stat(struct Fd *fd, struct stat *buf)
{
    return remfiled_stat(fd->fd_remfile.ino, buf);    
}

static int
remfile_probe(struct Fd *fd, dev_probe_t probe)
{
    return 0;
}

static int
remfile_close(struct Fd *fd)
{
    return 0;
}

           
struct Dev devremfile = {
    .dev_id = 'm',
    .dev_name = "remfile",
    .dev_read = &remfile_read,
    .dev_write = &remfile_write,
    .dev_probe = &remfile_probe,
    .dev_close = &remfile_close,
    .dev_stat = &remfile_stat,
};
