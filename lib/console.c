#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/fbcons.h>
#include <inc/kbdcodes.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <sys/stat.h>
#include <termios/kernel_termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/vt.h>
#include <linux/kd.h>

struct __kernel_termios __kernel_std_termios = {
    .c_iflag = ICRNL | IXON,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = B38400 | CS8 | CREAD | HUPCL,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,

    /*
     *  intr=^C         quit=^\         erase=del       kill=^U
     *  eof=^D          vtime=\0        vmin=\1         sxtc=\0
     *  start=^Q        stop=^S         susp=^Z         eol=\0
     *  reprint=^R      discard=^U      werase=^W       lnext=^V
     *  eol2=\0
     */
    .c_cc = "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0",
};

int
iscons(int fdnum)
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0)
	return r;
    return fd->fd_dev_id == devcons.dev_id ||
	   fd->fd_dev_id == devfbcons.dev_id;
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
    fd->fd_cons.ios = __kernel_std_termios;
    fd->fd_cons.ws.ws_row = 25;
    fd->fd_cons.ws.ws_col = 80;
    fd->fd_cons.ws.ws_xpixel = 0;
    fd->fd_cons.ws.ws_ypixel = 0;
    fd->fd_cons.pending_count = 0;
    return fd2num(fd);
}

static int
cons_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    return opencons();
}

static const char esc_codes[256] = {
    [KEY_INS]  = '2',
    [KEY_DEL]  = '3',
    [KEY_PGUP] = '5',
    [KEY_PGDN] = '6',
    [KEY_HOME] = '7',
    [KEY_END]  = '8',
    [KEY_UP]   = 'A',
    [KEY_DN]   = 'B',
    [KEY_RT]   = 'C',
    [KEY_LF]   = 'D',
};

static ssize_t
cons_read(struct Fd* fd, void* vbuf, size_t n, off_t offset)
{
    int c;
    char *cbuf = vbuf;

    if (n == 0)
	return 0;

 retry:
    if (!fd->fd_immutable && fd->fd_cons.pending_count) {
	ssize_t ncopy = MIN(n, fd->fd_cons.pending_count);
	memcpy(cbuf, fd->fd_cons.pending, ncopy);
	memmove(&fd->fd_cons.pending[0],
		&fd->fd_cons.pending[ncopy],
		fd->fd_cons.pending_count - ncopy);
	fd->fd_cons.pending_count -= ncopy;
	return ncopy;
    }

    c = sys_cons_getc();
    if (c < 0)
	return c;

    char esc = esc_codes[(uint8_t) c];
    if (!fd->fd_immutable && esc) {
	fd->fd_cons.pending[fd->fd_cons.pending_count++] = '\x1b';
	fd->fd_cons.pending[fd->fd_cons.pending_count++] = '[';
	fd->fd_cons.pending[fd->fd_cons.pending_count++] = esc;
	if (esc >= '0' && esc <= '9')
	    fd->fd_cons.pending[fd->fd_cons.pending_count++] = '~';
	goto retry;
    }

    cbuf[0] = c;
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
    if (probe == dev_probe_read) {
	if (fd->fd_cons.pending_count)
	    return 1;
	return sys_cons_probe();
    }

    return 1;
}

static int
cons_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    assert(fd->fd_isatty);

    switch (req) {
    case TCGETS: {
	struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	memcpy(k_termios, &fd->fd_cons.ios, sizeof(*k_termios));
	return 0;
    }

    case TIOCGWINSZ: {
	*(struct winsize *) va_arg(ap, struct winsize*) = fd->fd_cons.ws;
	return 0;
    }

    case TIOCGPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	*pgrp = fd->fd_cons.pgid;
	return 0;
    }

    case TIOCSPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	if (!fd->fd_immutable)
	    fd->fd_cons.pgid = *pgrp;
	return 0;
    }

    case VT_GETSTATE:
    case VT_ACTIVATE:  case VT_WAITACTIVE:
    case VT_GETMODE:   case VT_SETMODE:
	return 0;

    case KDGETMODE:    case KDSETMODE: {
	if (fd->fd_dev_id != devfbcons.dev_id)
	    return 0;

	struct fbcons_seg *fs = 0;
	uint64_t nbytes = sizeof(*fs);
	int r = segment_map(fd->fd_cons.fbcons_seg, 0,
			    SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &fs, &nbytes, 0);
	if (r < 0) {
	    __set_errno(EINVAL);
	    return -1;
	}

	jthread_mutex_lock(&fs->mu);

	if (req == KDGETMODE) {
	    int *modep = va_arg(ap, int*);
	    *modep = fs->stopped ? KD_GRAPHICS : KD_TEXT;
	} else if (req == KDSETMODE) {
	    int mode = va_arg(ap, int);
	    fs->stopped = (mode == KD_GRAPHICS) ? 1 : 0;
	    fs->redraw++;
	}

	fs->updates++;
	jthread_mutex_unlock(&fs->mu);
	sys_sync_wakeup(&fs->updates);

	segment_unmap_delayed(fs, 1);
	return 0;
    }

    default:
	return -1;
    }
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
		  volatile uint64_t **addrp, void **arg1)
{
    struct cons_statsync *thread_arg = malloc(sizeof(*thread_arg));
    jos_atomic_set(&thread_arg->ref, 2);
    thread_arg->wakeaddr = *addrp;

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

static int
cons_sync(struct Fd *fd)
{
    return 0;
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
    .dev_sync = cons_sync,
};

/*
 * Framebuffer-based console.
 */

static int
fbcons_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct fbcons_seg *fs = 0;
    uint64_t nbytes = 0;
    int r = segment_map(ino.obj, 0, SEGMAP_READ, (void **) &fs, &nbytes, 0);
    if (r < 0) {
	__set_errno(EIO);
	return -1;
    }

    if (nbytes < sizeof(*fs)) {
	segment_unmap(fs);
	__set_errno(EIO);
	return -1;
    }

    uint64_t taint = fs->taint;
    uint64_t grant = fs->grant;
    uint32_t rows = fs->rows;
    uint32_t cols = fs->cols;
    segment_unmap_delayed(fs, 1);

    struct Fd *fd;
    r = fd_alloc(&fd, "fbcons");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }

    fd->fd_dev_id = devfbcons.dev_id;
    fd->fd_omode = flags;
    fd->fd_isatty = 1;
    fd->fd_cons.pgid = getpgrp();
    fd->fd_cons.fbcons_seg = ino.obj;
    fd->fd_cons.ios = __kernel_std_termios;
    fd->fd_cons.ws.ws_row = rows;
    fd->fd_cons.ws.ws_col = cols;
    fd->fd_cons.ws.ws_xpixel = 0;
    fd->fd_cons.ws.ws_ypixel = 0;
    fd->fd_cons.pending_count = 0;
    fd->fd_cons.outbuf_count = 0;

    fd_set_extra_handles(fd, taint, grant);
    return fd2num(fd);
}

static void
fbcons_write_wc(struct fbcons_seg *fs, uint32_t wc)
{
    switch (wc) {
    case '\r':
	fs->xpos = 0;
	break;

    case '\n':
	fs->ypos++;
	fs->xpos = 0;
	break;

    case '\b':
	if (fs->xpos > 0)
	    fs->xpos--;
	break;

    case '\t':
	do {
	    fs->xpos++;
	} while (fs->xpos % 8);
	break;

    case '\a':
	/* no alarm */
	break;

    default:
	fs->data[fs->ypos * fs->cols + fs->xpos] = wc;
	fs->xpos++;
    }

    if (fs->xpos >= fs->cols) {
	fs->ypos++;
	fs->xpos = 0;
    }

    while (fs->ypos >= fs->rows) {
	memmove((void*) &fs->data[0], (void*) &fs->data[fs->cols],
		fs->cols * (fs->rows - 1) * sizeof(fs->data[0]));
	for (uint32_t i = 0; i < fs->cols; i++)
	    fs->data[fs->cols * (fs->rows - 1) + i] = ' ';
	fs->ypos--;
    }
}

static ssize_t
fbcons_write(struct Fd *fd, const void *buf, size_t len, off_t offset)
{
    ssize_t ret = -1;
    struct fbcons_seg *fs = 0;
    uint64_t nbytes = 0;
    int r = segment_map(fd->fd_cons.fbcons_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &fs, &nbytes, 0);
    if (r < 0) {
	__set_errno(EIO);
	goto err;
    }

    if (nbytes < sizeof(*fs) ||
	nbytes != sizeof(*fs) + fs->cols * fs->rows * sizeof(fs->data[0]) ||
	fs->xpos >= fs->cols || fs->ypos >= fs->rows)
    {
	__set_errno(EIO);
	goto err;
    }

    jthread_mutex_lock(&fs->mu);
    ret = 0;

    while (len > 0) {
	if (fd->fd_cons.outbuf_count < sizeof(fd->fd_cons.outbuf))
	    fd->fd_cons.outbuf[fd->fd_cons.outbuf_count++] = *(uint8_t*) buf;

	buf++;
	len--;
	ret++;

	while (fd->fd_cons.outbuf_count > 0) {
	    uint32_t wc = 0;
	    uint32_t cc = 0;

	    if ((fd->fd_cons.outbuf[0] & 0x80) == 0x00) {
		wc = fd->fd_cons.outbuf[0];
		cc = 1;
	    } else if ((fd->fd_cons.outbuf[0] & 0xe0) == 0xc0) {
		if (fd->fd_cons.outbuf_count < 2)
		    break;
		if ((fd->fd_cons.outbuf[1] & 0xc0) == 0x80) {
		    wc = (((uint32_t) fd->fd_cons.outbuf[0] & 0x1f) << 6) |
			  ((uint32_t) fd->fd_cons.outbuf[1] & 0x3f);
		    cc = 2;
		}
	    } else if ((fd->fd_cons.outbuf[0] & 0xf0) == 0xe0) {
		if (fd->fd_cons.outbuf_count < 3)
		    break;
		if (((fd->fd_cons.outbuf[1] & 0xc0) == 0x80) &&
		    ((fd->fd_cons.outbuf[2] & 0xc0) == 0x80))
		{
		    wc = (((uint32_t) fd->fd_cons.outbuf[0] & 0x0f) << 12) |
			 (((uint32_t) fd->fd_cons.outbuf[1] & 0x3f) << 6)  |
			  ((uint32_t) fd->fd_cons.outbuf[2] & 0x3f);
		    cc = 3;
		}
	    } else if ((fd->fd_cons.outbuf[0] & 0xf8) == 0xf0) {
		if (fd->fd_cons.outbuf_count < 4)
		    break;
		if (((fd->fd_cons.outbuf[1] & 0xc0) == 0x80) &&
		    ((fd->fd_cons.outbuf[2] & 0xc0) == 0x80) &&
		    ((fd->fd_cons.outbuf[3] & 0xc0) == 0x80))
		{
		    wc = (((uint32_t) fd->fd_cons.outbuf[0] & 0x07) << 18) |
			 (((uint32_t) fd->fd_cons.outbuf[1] & 0x3f) << 12) |
			 (((uint32_t) fd->fd_cons.outbuf[2] & 0x3f) << 6)  |
			  ((uint32_t) fd->fd_cons.outbuf[3] & 0x3f);
		    cc = 4;
		}
	    }

	    if (cc > 0) {
		memmove(&fd->fd_cons.outbuf[0], &fd->fd_cons.outbuf[cc],
			fd->fd_cons.outbuf_count - cc);
		fd->fd_cons.outbuf_count -= cc;
		fbcons_write_wc(fs, wc);
	    } else {
		/* Fallback: we display all bytes as-is */
		for (uint32_t i = 0; i < fd->fd_cons.outbuf_count; i++)
		    fbcons_write_wc(fs, fd->fd_cons.outbuf[i]);
		fd->fd_cons.outbuf_count = 0;
	    }
	}
    }

    fs->updates++;
    jthread_mutex_unlock(&fs->mu);
    sys_sync_wakeup(&fs->updates);
 err:
    if (fs)
	segment_unmap_delayed(fs, 1);
    return ret;
}

static ssize_t
fbcons_read(struct Fd* fd, void* vbuf, size_t n, off_t offset)
{
    ssize_t ret = -1;
    struct fbcons_seg *fs = 0;
    uint64_t nbytes = 0;
    int r = segment_map(fd->fd_cons.fbcons_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &fs, &nbytes, 0);
    if (r < 0) {
	__set_errno(EIO);
	goto err;
    }

    if (nbytes < sizeof(*fs)) {
	__set_errno(EIO);
	goto err;
    }

    jthread_mutex_lock(&fs->mu);
    ret = 0;

    uint8_t *buf = vbuf;
    while (n > (size_t) ret) {
	if (!fs->incount) {
	    if (ret)
		break;

	    jthread_mutex_unlock(&fs->mu);
	    sys_sync_wait(&fs->incount, 0, UINT64(~0));
	    jthread_mutex_lock(&fs->mu);
	    continue;
	}

	uint64_t inpos = fs->inpos;
	if (inpos >= sizeof(fs->inbuf))
	    break;

	buf[ret++] = fs->inbuf[inpos];
	fs->inpos = (inpos + 1) % sizeof(fs->inbuf);
	fs->incount--;
    }

    jthread_mutex_unlock(&fs->mu);
 err:
    if (fs)
	segment_unmap_delayed(fs, 1);
    return ret;
}

static int
fbcons_statsync(struct Fd *fd, dev_probe_t probe,
	      struct wait_stat *wstat, int wslot_avail)
{
    if (probe == dev_probe_write)
	return -1;

    if (wslot_avail < 1)
	return -1;

    memset(wstat, 0, sizeof(*wstat));
    WS_SETOBJ(wstat, fd->fd_cons.fbcons_seg,
	      offsetof(struct fbcons_seg, incount));
    WS_SETVAL(wstat, 0);
    wstat->ws_probe = probe;
    return 1;
}

static int
fbcons_probe(struct Fd *fd, dev_probe_t probe)
{
    if (probe == dev_probe_write)
	return 1;

    int ret = -1;
    struct fbcons_seg *fs = 0;
    uint64_t nbytes = 0;
    int r = segment_map(fd->fd_cons.fbcons_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &fs, &nbytes, 0);
    if (r < 0) {
	__set_errno(EIO);
	return -1;
    }
    
    if (nbytes < sizeof(*fs)) {
	__set_errno(EIO);
    } else {
	ret = fs->incount ? 1 : 0;
    }
 
    segment_unmap_delayed(fs, 1);
    return ret;
}

struct Dev devfbcons =
{
    .dev_id = 'C',
    .dev_name = "fbcons",
    .dev_open = fbcons_open,
    .dev_write = fbcons_write,
    .dev_read = fbcons_read,
    .dev_statsync = fbcons_statsync,
    .dev_probe = fbcons_probe,

    .dev_close = cons_close,
    .dev_ioctl = cons_ioctl,
    .dev_sync = cons_sync,
};
