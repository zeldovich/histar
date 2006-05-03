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

int remfile_open(char *host, char *path);

int
remfile_open(char *host, char *path)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "remfile");
    if (r < 0) {
        errno = ENOMEM;
        return -1;
    }
    return fd2num(fd);
}


static ssize_t
remfile_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return 0;
}

static ssize_t
remfile_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return 0;    
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
};
