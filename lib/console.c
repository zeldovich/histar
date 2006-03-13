#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

int
iscons(int fdnum)
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0)) < 0)
	return r;
    return fd->fd_dev_id == devcons.dev_id;
}

int
opencons(void)
{
    int r;
    struct Fd* fd;

    if ((r = fd_alloc(start_env->shared_container, &fd, "console fd")) < 0)
	return r;
    fd->fd_dev_id = devcons.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_immutable = 1;
    fd->fd_isatty = 1;
    return fd2num(fd);
}

static ssize_t
cons_read(struct Fd* fd, void* vbuf, size_t n, off_t offset)
{
    int c;

    if (n == 0)
	return 0;

    while ((c = sys_cons_getc()) == 0)
	sys_thread_yield();
    if (c < 0)
	return c;
    if (c == 0x04)	// ctl-d is eof
	return 0;
    *(char*)vbuf = c;
    return 1;
}

//#include <lib/vt/vt.h>

static ssize_t
cons_write(struct Fd *fd, const void *vbuf, size_t n, off_t offset)
{
    sys_cons_puts(vbuf, n);
    //vt_write(vbuf, n, offset) ;
    return n;
}

static int
cons_close(struct Fd *fd)
{
    return 0;
}

static int
cons_stat(struct Fd *fd, struct stat *buf)
{
    memset(buf, 0, sizeof(*buf));
    return 0;
}

struct Dev devcons =
{
    .dev_id = 'c',
    .dev_name = "cons",
    .dev_read = cons_read,
    .dev_write = cons_write,
    .dev_close = cons_close,
    .dev_stat = cons_stat,
};
