#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <termios/kernel_termios.h>
#include <sys/ioctl.h>
#include <lib/vt/vt.h>

static int enable_vt = 0 ;

int
iscons(int fdnum)
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0)
	return r;
    return fd->fd_dev_id == devcons.dev_id;
}

int
opencons(void)
{
    int r;
    struct Fd* fd;

    if ((r = fd_alloc(&fd, "console fd")) < 0)
	return r;

    fd->fd_dev_id = devcons.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_isatty = 1;

    r = fd_make_public(fd2num(fd), 0);
    if (r < 0) {
	cprintf("opencons: cannot make public: %s\n", e2s(r));
	jos_fd_close(fd);
	return r;
    }

    fd->fd_immutable = 1;
    return fd2num(fd);
}

static ssize_t
cons_read(struct Fd* fd, void* vbuf, size_t n, off_t offset)
{
    int c;

    if (n == 0)
	return 0;

    c = sys_cons_getc();
    if (c < 0)
	return c;
    if (c == 0x04)	// ctl-d is eof
	return 0;
    *(char*)vbuf = c;
    return 1;
}

static ssize_t
cons_write(struct Fd *fd, const void *vbuf, size_t n, off_t offset)
{
    if (enable_vt)
        vt_write(vbuf, n, offset) ;
    else    
        sys_cons_puts(vbuf, n);
    return n;
}

static int
cons_close(struct Fd *fd)
{
    return 0;
}

static int
cons_probe(struct Fd *fd, dev_probe_t probe)
{
    if (probe == dev_probe_read)
        return sys_cons_probe();
    return 1 ;
}

static int
cons_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    assert(fd->fd_isatty);
    if (req == TCGETS) {
	struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	if (k_termios)
	    memset(k_termios, 0, sizeof(*k_termios));
	
	// XXX 
	k_termios->c_lflag |= ECHO;
	return 0;
    }
    return -1;
}

struct Dev devcons =
{
    .dev_id = 'c',
    .dev_name = "cons",
    .dev_read = cons_read,
    .dev_write = cons_write,
    .dev_close = cons_close,
    .dev_probe = cons_probe,
    .dev_ioctl = cons_ioctl,
};
