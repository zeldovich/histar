#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>

int
putchar(int ch)
{
    unsigned char c = ch;
    write(1, &c, 1);
    return c;
}

int
getchar(void)
{
    unsigned char c;
    int r = read(0, &c, 1);
    if (r < 0)
	return r;
    if (r == 0)
	return -E_EOF;
    return c;
}

int
iscons(int fdnum)
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd)) < 0)
	return r;
    return fd->fd_dev_id == devcons.dev_id;
}

int
opencons(uint64_t container)
{
    int r;
    struct Fd* fd;

    if ((r = fd_alloc(container, &fd, "console fd")) < 0)
	return r;
    fd->fd_dev_id = devcons.dev_id;
    fd->fd_omode = O_RDWR;
    return fd2num(fd);
}

static int
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

int
cons_write(struct Fd *fd, const void *vbuf, size_t n, off_t offset)
{
    sys_cons_puts(vbuf, n);
    return n;
}

int
cons_close(struct Fd *fd)
{
    return 0;
}

struct Dev devcons =
{
    .dev_id = 'c',
    .dev_name = "cons",
    .dev_read = cons_read,
    .dev_write = cons_write,
    .dev_close = cons_close,
};
