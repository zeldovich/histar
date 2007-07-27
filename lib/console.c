#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <termios/kernel_termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

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
    fd->fd_cons.pgid = getpgrp();
    
    r = fd_make_public(fd2num(fd), 0);
    if (r < 0) {
	cprintf("opencons: cannot make public: %s\n", e2s(r));
	jos_fd_close(fd);
	return r;
    }

    fd->fd_immutable = 1;
    return fd2num(fd);
}

static int
cons_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    return opencons();
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
    else if (req == TIOCGPGRP) {
	pid_t *pgrp = va_arg(ap, pid_t *);
	*pgrp = fd->fd_cons.pgid;
	return 0;
    } else if (req == TIOCSPGRP) {
	pid_t *pgrp = va_arg(ap, pid_t *);
	if (!fd->fd_immutable)
	    fd->fd_cons.pgid = *pgrp;
	return 0;
    }
    return -1;
}

struct cons_statsync {
    jos_atomic_t ref;
    volatile uint64_t *wakeaddr;
};

static void
cons_statsync_thread(void *arg)
{
    struct cons_statsync *args = arg;

    while (jos_atomic_read(&args->ref) > 1) {
	int r = sys_cons_probe();
	if (r >= 0) {
	    (*args->wakeaddr)++;
	    sys_sync_wakeup(args->wakeaddr);
	    break;
	}
	usleep(50000);
    }

    if (jos_atomic_dec_and_test(&args->ref))
	free(args);
}

static int
cons_statsync_cb0(struct wait_stat *ws, dev_probe_t probe,
		  volatile uint64_t *addr, void **arg1)
{
    struct cons_statsync *thread_arg = malloc(sizeof(*thread_arg));
    jos_atomic_set(&thread_arg->ref, 2);
    thread_arg->wakeaddr = addr;

    struct cobj_ref tobj;
    thread_create(start_env->proc_container, cons_statsync_thread,
		  thread_arg, &tobj, "cons select thread");

    *arg1 = thread_arg;
    return 0;
}

static int
cons_statsync_cb1(struct wait_stat *ws, void *arg1, dev_probe_t probe)
{
    struct cons_statsync *args = arg1;
    if (jos_atomic_dec_and_test(&args->ref))
	free(args);
    
    return 0;
}

static int
cons_statsync(struct Fd *fd, dev_probe_t probe,
	      struct wait_stat *wstat, int wslot_avail)
{
    if (probe == dev_probe_write)
	return -1;

    if (wslot_avail < 1)
	return -1;

    WS_SETASS(wstat);
    WS_SETCB0(wstat, &cons_statsync_cb0);
    WS_SETCB1(wstat, &cons_statsync_cb1);
    wstat->ws_probe = probe;

    return 1;
}

struct Dev devcons =
{
    .dev_id = 'c',
    .dev_name = "cons",
    .dev_read = cons_read,
    .dev_write = cons_write,
    .dev_open = cons_open,
    .dev_close = cons_close,
    .dev_probe = cons_probe,
    .dev_ioctl = cons_ioctl,
    .dev_statsync = cons_statsync,
};
