#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/jcomm.h>
#include <inc/stdio.h>
#include <inc/pty.h>
#include <inc/labelutil.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <inc/setjmp.h>

#include <bits/unimpl.h>

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/stropts.h>

#include <linux/vt.h>
#include <linux/kd.h>

/* XXX dependent on fork.cc */
#define PTY_CT start_env->shared_container
#define PTY_JCOMM(fd) JCOMM(PTY_CT, fd->fd_pty.pty_jc)
#define PTY_SEG(fd) COBJ(PTY_CT, fd->fd_pty.pty_seg)

static int
pty_addref(struct Fd *fd, uint64_t ct)
{
    int r = jcomm_addref(PTY_JCOMM(fd), ct);
    if (r < 0) {
	/* this is probably a spurious warning, remove if annoying */
	cprintf("pty_addref: jcomm_addref error: %s\n", e2s(r));
	return r;
    }

    r = sys_segment_addref(PTY_SEG(fd), ct);
    if (r < 0)
	cprintf("pty_addref: sys_segment_addref error: %s\n", e2s(r));
    return r;
}

static int
pty_unref(struct Fd *fd)
{
    int r = jcomm_unref(PTY_JCOMM(fd));
    if (r < 0) {
	cprintf("pty_unref: jcomm_unref error: %s\n", e2s(r));
	return r;
    }
    r = sys_obj_unref(PTY_SEG(fd));
    if (r < 0)
	cprintf("pty_unref: sys_obj_unref error: %s\n", e2s(r));
    return r;
}

static int
ptm_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "ptm fd");
    if (r < 0) {
	__set_errno(ENOMEM);
	return -1;
    }
    
    fd->fd_omode = flags;
    fd->fd_dev_id = devptm.dev_id;
    fd->fd_isatty = 1;
    fd->fd_omode = O_RDWR;
    
    int64_t taint = handle_alloc();
    int64_t grant = handle_alloc();
    
    uint64_t ents[4];
    memset(ents, 0, sizeof(ents));
    struct ulabel label = { .ul_size = 4, .ul_ent = ents,
			    .ul_nent = 0, .ul_default = 1 } ;
    label_set_level(&label, taint, 3, 0);
    label_set_level(&label, grant, 0, 0);

    struct jcomm_ref master_jr, slave_jr;
    r = jcomm_alloc(PTY_CT, &label, 0, &master_jr, &slave_jr);

    struct pty_seg *ps = 0;
    struct cobj_ref pty_seg_cobj;
    r = segment_alloc(start_env->shared_container,
		      sizeof(*ps), &pty_seg_cobj,
		      (void **)&ps, &label, "pty-seg");
    if (r < 0) {
	jos_fd_close(fd);
	errno = ENOMEM;
	return -1;        
    }

    struct fs_object_meta m;
    m.dev_id = devpts.dev_id;
    r = sys_obj_set_meta(pty_seg_cobj, 0, &m);
    if (r < 0) {
	jos_fd_close(fd);
	errno = ENOMEM;
	return -1;
    }

    sys_obj_set_fixedquota(pty_seg_cobj);
    memset(ps, 0, sizeof(*ps));

    ps->slave_jc = slave_jr.jc;
    ps->grant = grant;
    ps->taint = taint;
    ps->ios = __kernel_std_termios;

    fd->fd_pty.pty_jc = master_jr.jc;
    fd->fd_pty.pty_seg = pty_seg_cobj.object;
    segment_unmap_delayed(ps, 1);

    /* For another thread to use the slave, it must have grant and taint.
     * Setting the extra handles should work for most UNIX processes,
     * because openpty(*master_fd, *slave_fd, ...) is usually called before 
     * a fork, then the child closes the master and the parent closes the 
     * slave.
     */ 
    fd_set_extra_handles(fd, grant, taint);
    return fd2num(fd);
}

static int
pts_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "pts fd");
    if (r < 0) {
	__set_errno(ENOMEM);
	return -1;
    }

    fd->fd_omode = flags;
    fd->fd_dev_id = devpts.dev_id;
    fd->fd_isatty = 1;
    fd->fd_omode = O_RDWR;

    struct pty_seg *ps = 0;
    r = segment_map(ino.obj, 0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **) &ps, 0, 0);
    if (r < 0)
	goto out;

    fd->fd_pty.pty_jc = ps->slave_jc;
    fd->fd_pty.pty_seg = ino.obj.object;
    fd_set_extra_handles(fd, ps->taint, ps->grant);

    r = pty_addref(fd, PTY_CT);
    if (r < 0)
	goto out;

    /* ps->ref counts the number of pts struct Fds that are open.  So,
     * it only gets incremented when a new slave Fd is allocated, and
     * not by pty_addref.
     */
    jos_atomic_inc(&ps->ref);
    segment_unmap_delayed(ps, 1);
    return fd2num(fd);

 out:
    if (ps)
	segment_unmap_delayed(ps, 1);

    fd->fd_pty.pty_seg = 0;
    jos_fd_close(fd);
    errno = EACCES;
    return -1;
}

static int
pts_close(struct Fd *fd)
{
    /* a 'partially' allocated pts */
    if (!fd->fd_pty.pty_seg)
	return 0;

    volatile struct jos_jmp_buf *pf_old = tls_data->tls_pgfault;
    struct pty_seg *ps = 0;

    struct jos_jmp_buf pf_jb;
    if (jos_setjmp(&pf_jb) != 0)
	goto err;
    tls_data->tls_pgfault = &pf_jb;

    int r = segment_map(PTY_SEG(fd), 0,
			SEGMAP_READ | SEGMAP_WRITE | SEGMAP_VECTOR_PF,
			(void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pts_close: unable to map pty_seg: %s\n", e2s(r));
	goto err;
    }

    if (jos_atomic_dec_and_test(&ps->ref)) {
	r = jcomm_shut(PTY_JCOMM(fd), JCOMM_SHUT_RD | JCOMM_SHUT_WR);
	if (r < 0)
	    cprintf("pts_close: jcomm_shut error: %s\n", e2s(r));
    }

    segment_unmap_delayed(ps, 1);
    tls_data->tls_pgfault = pf_old;
    return pty_unref(fd);

 err:
    tls_data->tls_pgfault = pf_old;
    errno = EACCES;
    return -1;
}

static int
ptm_close(struct Fd *fd)
{
    int r = jcomm_shut(PTY_JCOMM(fd), JCOMM_SHUT_RD | JCOMM_SHUT_WR);
    if (r < 0)
	cprintf("ptm_close: jcomm_shut error: %s\n", e2s(r));

    return pty_unref(fd);
}

static ssize_t
pty_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    ssize_t ret = -1;
    char bf[count * 2];
    const char *ch = ((const char *)buf);
    uint32_t cc = 0;

    volatile struct jos_jmp_buf *pf_old = tls_data->tls_pgfault;
    struct pty_seg *ps = 0;

    struct jos_jmp_buf pf_jb;
    if (jos_setjmp(&pf_jb) != 0)
	goto err;
    tls_data->tls_pgfault = &pf_jb;

    int r = segment_map(PTY_SEG(fd), 0,
			SEGMAP_READ | SEGMAP_WRITE | SEGMAP_VECTOR_PF,
			(void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pty_write: unable to map pty_seg: %s\n", e2s(r));
	goto err;
    }

    uint32_t i = 0;
    for (; i < count; i++) {
	if (fd->fd_dev_id == devpts.dev_id) {
	    if ((ps->ios.c_oflag & ONLCR) && ch[i] == '\n') {
		bf[cc] = '\r';
		cc++;
	    }
	}

        if (fd->fd_dev_id == devptm.dev_id) {
            if (ps->ios.c_cc[VINTR] == ch[i]) {
                killpg(ps->pgrp, SIGINT);
                continue;
            }
	    if (ps->ios.c_cc[VSUSP] == ch[i]) {
                killpg(ps->pgrp, SIGTSTP);
                continue;
            }
	    /* if master->slave but none above match just fall through */
        }
        bf[cc] = ch[i];
        cc++;
    }
    
    r = jcomm_write(PTY_JCOMM(fd), bf, cc, 1);
    if (r < 0) {
	__set_errno(EIO);
	goto out;
    }

    /* lots of code assumes a write to stdout writes all bytes */
    while ((uint32_t) r < cc) {
	int rr = jcomm_write(PTY_JCOMM(fd), bf + r, cc - r, 1);
	if (rr < 0) {
	    __set_errno(EIO);
	    goto out;
	}
	r += rr;
    }
    assert((uint32_t)r == cc);
    ret = count;
    goto out;

 err:
    errno = EACCES;
 out:
    if (ps)
	segment_unmap_delayed(ps, 1);
    tls_data->tls_pgfault = pf_old;
    return ret;
}

static ssize_t
pty_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    ssize_t r = jcomm_read(PTY_JCOMM(fd), buf, count,
			   !(fd->fd_omode & O_NONBLOCK));
    if (r == -E_AGAIN) {
	__set_errno(EAGAIN);
	return -1;
    }

    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    return r;
}

static ssize_t
pts_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    int64_t pty_pgrp;
    ioctl(fd2num(fd), TIOCGPGRP, &pty_pgrp);
    if (pty_pgrp != getpgrp())
        kill(0, SIGTTIN);
    return pty_read(fd, buf, count, offset);
}

static int
pty_probe(struct Fd *fd, dev_probe_t probe)
{
    return jcomm_probe(PTY_JCOMM(fd), probe);
}

static int
pty_statsync(struct Fd *fd, dev_probe_t probe,
	     struct wait_stat *wstat, int wslot_avail)
{
    return jcomm_multisync(PTY_JCOMM(fd), probe, wstat, wslot_avail);
}

static int
pty_stat(struct Fd *fd, struct stat64 *buf)
{
    buf->st_mode |= S_IFCHR;
    return 0;
}

static int
pty_shutdown(struct Fd *fd, int how)
{
    int16_t h = 0;
    if (how & SHUT_RD)
	h |= JCOMM_SHUT_RD;
    if (how & SHUT_WR)
	h |= JCOMM_SHUT_WR;
    if (how & SHUT_RDWR)
	h |= JCOMM_SHUT_RD | JCOMM_SHUT_WR;
    int r = jcomm_shut(PTY_JCOMM(fd), h);
    if (r < 0)
	cprintf("pty_shutdown: jcomm_shut error: %s\n", e2s(r));
    return r;
}

static int
pty_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    int ret = -1;

    volatile struct jos_jmp_buf *pf_old = tls_data->tls_pgfault;
    struct pty_seg *ps = 0;

    struct jos_jmp_buf pf_jb;
    if (jos_setjmp(&pf_jb) != 0)
	goto err;
    tls_data->tls_pgfault = &pf_jb;

    int r = segment_map(PTY_SEG(fd), 0,
			SEGMAP_READ | SEGMAP_WRITE | SEGMAP_VECTOR_PF,
			(void **)&ps, 0, 0);
    if (r < 0)
	goto err;

    switch (req) {
    case VT_GETSTATE:
    case VT_ACTIVATE:  case VT_WAITACTIVE:
    case VT_GETMODE:   case VT_SETMODE:
    case KDGETMODE:    case KDSETMODE:
	return 0;

    case TCGETS: {
    	if (!fd->fd_isatty) {
	    __set_errno(ENOTTY);
	    goto out;
    	}

	struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	memcpy(k_termios, &ps->ios, sizeof(*k_termios));
	ret = 0;
	break;
    }

    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
	const struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	memcpy(&ps->ios, k_termios, sizeof(ps->ios));
	ret = 0;
	break;
    }

    case TIOCGPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	*pgrp = ps->pgrp;
	ret = 0;
	break;
    }

    case TIOCSPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	ps->pgrp = *pgrp;
	ret = 0;
	break;
    }

    case TIOCSCTTY:
        /* Set processes controlling tty as this pty and update pgrp */
        ps->pgrp = getpgrp();
        start_env->ctty = fd->fd_pty.pty_seg;
	ret = 0;
	break;

    case TIOCNOTTY:
        /* Disassociate this pty from its controlling tty */
	start_env->ctty = 0;
	ret = 0;
	break;

    case TIOCSWINSZ:
	ps->winsize = *(struct winsize *) va_arg(ap, struct winsize*);
	killpg(ps->pgrp, SIGWINCH);
	ret = 0;
	break;

    case TIOCGWINSZ:
	*(struct winsize *) va_arg(ap, struct winsize*) = ps->winsize;
	ret = 0;
	break;

    case TIOCGPTN:
	cprintf("pty_ioctl: TIOCGPTN not supported\n");
	__set_errno(E2BIG);
	ret = -1;
	break;

    case TIOCSPTLCK:
	/* the pts associated with fd is always unlocked */
	ret = 0;
	break;

    case I_PUSH:
	__set_errno(EINVAL);
	ret = -1;
	break;

    default:
	cprintf("pty_ioctl: request 0x%"PRIx64" unimplemented\n", req);
	__set_errno(EINVAL);
    }

    goto out;

 err:
    errno = EACCES;

 out:
    if (ps)
	segment_unmap_delayed(ps, 1);
    tls_data->tls_pgfault = pf_old;
    return ret;
}

static int
pty_sync(struct Fd *fd)
{
    return 0;
}

static int
tty_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    if (!start_env->ctty) {
        /* If no ctty was specified then just return the console this probably
           isn't correct, we should probably have init set the console as its
           childrens' controlling tty and return error if it wasn't set at all,
           but it seems like this should work in general as long as the console
           doesn't go away. */
        return opencons();
    }

    char pnbuf[128];
    sprintf(&pnbuf[0], "#%"PRIu64".%"PRIu64, PTY_CT, start_env->ctty);
    return open(&pnbuf[0], O_RDWR);
}

libc_hidden_proto(ptsname_r)

int
ptsname_r(int fdnum, char *buf, size_t buflen)
{
    struct Fd *fd;
    int r = fd_lookup(fdnum, &fd, 0, 0);
    if (r < 0) {
	__set_errno(EBADF);
	return -1;
    }

    if (fd->fd_dev_id != devptm.dev_id) {
	__set_errno(ENOTTY);
	return -1;
    }

    snprintf(buf, buflen, "#%"PRIu64".%"PRIu64, PTY_CT, fd->fd_pty.pty_seg);
    return 0;
}

libc_hidden_def(ptsname_r)

char *
ptsname(int fd)
{
    static char buf[256];
    return ptsname_r(fd, buf, sizeof(buf)) ? 0 : buf;
}

struct Dev devtty = {
    .dev_id = 'w',
    .dev_name = "tty",
    .dev_open = tty_open,
};

struct Dev devptm = {
    .dev_id = 'x',
    .dev_name = "ptm",
    .dev_read = pty_read,
    .dev_write = pty_write,
    .dev_open = ptm_open,
    .dev_close = ptm_close,
    .dev_stat = pty_stat,
    .dev_probe = pty_probe,
    .dev_statsync = pty_statsync,
    .dev_shutdown = pty_shutdown,
    .dev_ioctl = pty_ioctl,
    .dev_addref = &pty_addref,
    .dev_unref = &pty_unref,
    .dev_sync = &pty_sync,
};

struct Dev devpts = {
    .dev_id = 'y',
    .dev_name = "pts",
    .dev_read = pts_read,
    .dev_write = pty_write,
    .dev_open = pts_open,
    .dev_close = pts_close,
    .dev_stat = pty_stat,
    .dev_probe = pty_probe,
    .dev_statsync = pty_statsync,
    .dev_shutdown = pty_shutdown,
    .dev_ioctl = pty_ioctl,
    .dev_addref = &pty_addref,
    .dev_unref = &pty_unref,
    .dev_sync = &pty_sync,
};
