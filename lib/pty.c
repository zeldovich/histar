#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/jcomm.h>
#include <inc/stdio.h>
#include <inc/pty.h>
#include <inc/labelutil.h>
#include <inc/syscall.h>

#include <bits/ptyhelper.h>
#include <bits/unimpl.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/ioctl.h>

/* Note: pty Fd slaves use an additional segment (pty_seg) for their 
 * meta data, instead of storing it in the Fd segment like pty masters.
 * This is done so pty masters can read and write slave meta data.
 * This is unnecessary in the current impl, but would be required for
 * some features, like 'ctrl+c'.
 */

/* XXX dependent on fork.cc */
#define PTY_CT start_env->shared_container
#define PTY_JCOMM(fd) JCOMM(PTY_CT, fd->fd_pty.pty_jc)
#define PTY_SLAVE(fd) COBJ(PTY_CT, fd->fd_pty.pty_slave_seg.object)

static int
pty_addref(struct Fd *fd, uint64_t ct)
{
    int r = jcomm_addref(PTY_JCOMM(fd), ct);
    if (r < 0) {
	/* this is probably a spurious warning, remove if annoying */
	cprintf("pty_addref: jcomm_addref error: %s\n", e2s(r));
	return r;
    }
    r = sys_segment_addref(PTY_SLAVE(fd), ct);
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
    r = sys_obj_unref(PTY_SLAVE(fd));
    if (r < 0)
	cprintf("pty_unref: sys_obj_unref error: %s\n", e2s(r));
    return r;
}

static int
ptm_open(struct fs_inode ino, int flags, uint32_t dev_opt)
{
    struct pts_descriptor pd;
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
    
    uint64_t ct = start_env->shared_container;
    
    struct jcomm_ref master_jr, slave_jr;
    r = jcomm_alloc(PTY_CT, &label, 0, &master_jr, &slave_jr);
   
    struct pty_seg *ps = 0;
    struct cobj_ref slave_pty_seg;
    if ((r = segment_alloc(ct, sizeof(*ps), &slave_pty_seg, 
			   (void **)&ps, &label, "pty-slave-ios")) < 0) {
	jos_fd_close(fd);
	errno = ENOMEM;
	return -1;        
    }
    sys_obj_set_fixedquota(slave_pty_seg);
    memset(ps, 0, sizeof(*ps));

    fd->fd_pty.pty_jc = master_jr.jc;
    fd->fd_pty.pty_slave_seg = slave_pty_seg;

    segment_unmap_delayed(ps, 1);

    pd.slave_pty_seg = slave_pty_seg;
    pd.slave_jc = slave_jr.jc;
    pd.grant = grant;
    pd.taint = taint;
    r = pty_alloc(&pd);
    if (r < 0) {
	jos_fd_close(fd);
	__set_errno(ENOTSUP);
	return -1;
    }
    fd->fd_pty.pty_no = r;
    
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
    int ptyno = (int) dev_opt;

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

    struct pts_descriptor pd;
    r = pty_lookup(ptyno, &pd);
    
    if (r < 0) {
	jos_fd_close(fd);
	errno = ENOTSUP;
	return -1;
    }
  
    fd->fd_pty.pty_no = ptyno;
    fd->fd_pty.pty_jc = pd.slave_jc;
    fd->fd_pty.pty_slave_seg = pd.slave_pty_seg;
    fd_set_extra_handles(fd, pd.taint, pd.grant);

    /* common for processes (via libc) to open pty slaves they don't
     * have the priv. for.  Note, we will close a 'parially' allocated 
     * fd.
     */
    
    r = pty_addref(fd, PTY_CT);
    if (r < 0) {
	memset(&fd->fd_pty, 0, sizeof(fd->fd_pty));
	jos_fd_close(fd);
	errno = EACCES;
	return -1;
    }

    struct pty_seg *ps = 0;
    r = segment_map(PTY_SLAVE(fd), 0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **)&ps, 0, 0);
    if (r < 0) {
	memset(&fd->fd_pty, 0, sizeof(fd->fd_pty));
	jos_fd_close(fd);
	errno = EACCES;
	return -1;
    }

    /* ps->ref counts the number of pts struct Fds that are open.  So,
     * it only gets incremented when a new slave Fd is allocated, and
     * not by pty_addref.
     */
    jos_atomic_inc(&ps->ref);
    segment_unmap_delayed(ps, 1);
    return fd2num(fd);
}

static int
pts_close(struct Fd *fd)
{
    /* a 'partially' allocated pts */
    if (!fd->fd_pty.pty_slave_seg.object) 
	return 0;

    struct pty_seg *ps = 0;
    int r = segment_map(PTY_SLAVE(fd), 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pts_close: unable to map pty_seg: %s\n", e2s(r));
	errno = EACCES;
	return -1;
    }
    
    if (jos_atomic_dec_and_test(&ps->ref)) {
	r = jcomm_shut(PTY_JCOMM(fd), JCOMM_SHUT_RD | JCOMM_SHUT_WR);
	if (r < 0)
	    cprintf("pts_close: jcomm_shut error: %s\n", e2s(r));
    }
    segment_unmap_delayed(ps, 1);
        
    return pty_unref(fd);
}

static int
ptm_close(struct Fd *fd)
{
    int r = jcomm_shut(PTY_JCOMM(fd), JCOMM_SHUT_RD | JCOMM_SHUT_WR);
    if (r < 0)
	cprintf("ptm_close: jcomm_shut error: %s\n", e2s(r));

    pty_remove(fd->fd_pty.pty_no);
    return pty_unref(fd);
}

static uint32_t
pty_handle_nl(struct Fd *fd, char *buf, tcflag_t flags)
{
    if (flags & ONLCR) {
	buf[0] = '\r';
	buf[1] = '\n';
	return 2;
    } else {
	buf[0] = '\n';
	return 1;
    }
}

static ssize_t
pty_write(struct Fd *fd, const void *buf, size_t count, struct pty_seg *ps)
{
    char bf[count * 2];
    const char *ch = ((const char *)buf);
    uint32_t cc = 0;
	
    uint32_t i = 0;
    for (; i < count; i++) {
        switch (ch[i]) {
	case '\n':
	    cc += pty_handle_nl(fd, &bf[cc], ps->ios.c_oflag);
	    break;
	default:
	    /*
	      if (fd->fd_pt.is_master && otermios->c_cc[VINTR] == ch[i])
	          kill(oios->pgrp, SIGINT);
	      else
	    */

	    bf[cc] = ch[i];
	    cc++;
	    break;
	}
    }
    
    int r = jcomm_write(PTY_JCOMM(fd), bf, cc);
    if (r < 0) {
	cprintf("pty_write: jcomm_write failed: %s\n", e2s(r));
	__set_errno(EIO);
	return -1;
    }

    /* lots of code assumes a write to stdout writes all bytes */
    while ((uint32_t) r < cc) {
	int fdnum = fd2num(fd);

	struct timeval tv = {1, 0};
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	
	fd_set writeset;
	FD_ZERO(&writeset);
	FD_SET(fdnum, &writeset);
	
	int n = select(fdnum + 1, 0, &writeset, 0, &tv);
	if (n == 0) {
	    cprintf("pty_write: only able to write %d out of %zu\n", r, count);
	    __set_errno(EIO);
	    return -1;
	}

	int rr = jcomm_write(PTY_JCOMM(fd), bf + r, cc - r);
	if (rr < 0) {
	    cprintf("pty_write: error on jcomm write: %s\n", e2s(rr));
	    __set_errno(EIO);
	    return -1;
	}
	r += rr;
    }
    assert((uint32_t)r == cc);
    return count;
}

static ssize_t
ptm_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return pty_write(fd, buf, count, &fd->fd_pty.ptm_ps);
}

static ssize_t
pts_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    struct pty_seg *ps = 0;
    int r = segment_map(PTY_SLAVE(fd), 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pts_write: unable to map pty_seg: %s\n", e2s(r));
	return -1;
    }
    
    r = pty_write(fd, buf, count, ps);
    segment_unmap_delayed(ps, 1);
    return r;
}

static ssize_t
pty_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    int r = jcomm_read(PTY_JCOMM(fd), buf, count);
    if (r < 0)
	cprintf("pty_read: jcomm_read error: %s\n", e2s(r));
    return r;
}

static int
pty_probe(struct Fd *fd, dev_probe_t probe)
{
    return jcomm_probe(PTY_JCOMM(fd), probe);
}

static int
pty_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{
    return jcomm_multisync(PTY_JCOMM(fd), probe, wstat);
}

static int
pts_stat(struct Fd *fd, struct stat64 *buf)
{
    buf->st_mode |= S_IFCHR;
    buf->st_rdev = fd->fd_pty.pty_no;
    return 0;
}

static int
ptm_stat(struct Fd *fd, struct stat64 *buf)
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
pty_ioctl(struct Fd *fd, uint64_t req, va_list ap, struct pty_seg *ps)
{
    switch (req) {
    case TCGETS: {
    	if (!fd->fd_isatty) {
	    __set_errno(ENOTTY);
	    return -1;
    	}

	struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	memcpy(k_termios, &ps->ios, sizeof(*k_termios));
	return 0;
    }

    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
	const struct __kernel_termios *k_termios;
	k_termios = va_arg(ap, struct __kernel_termios *);
	memcpy(&ps->ios, k_termios, sizeof(ps->ios));
	return 0;
    }

    case TIOCGPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	*pgrp = ps->pgrp;
	return 0;
    }

    case TIOCSPGRP: {
	pid_t *pgrp = va_arg(ap, pid_t *);
	ps->pgrp = *pgrp;
	return 0;
    }

    case TIOCSCTTY:
	ps->pgrp = getpgrp();
	return 0;

    case TIOCSWINSZ:
	ps->winsize = *(struct winsize *) va_arg(ap, struct winsize*);
	/*
	 * In theory, should send SIGWINCH to the process group.
	 */
	return 0;

    case TIOCGWINSZ:
	*(struct winsize *) va_arg(ap, struct winsize*) = ps->winsize;
	return 0;

    default:
	cprintf("pty_ioctl: request 0x%"PRIx64" unimplemented\n", req);
	return -1;
    }
}

static int
ptm_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    if (req == TIOCGPTN) {
	int *ptyno = va_arg(ap, int *);
	*ptyno = fd->fd_pty.pty_no;
	return 0;
    } if (req == TIOCSPTLCK) {
	/* the pts associated with fd is always unlocked */
	return 0;
    } 

    return pty_ioctl(fd, req, ap, &fd->fd_pty.ptm_ps);
}

static int
pts_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    struct pty_seg *ps = 0;
    int r = segment_map(PTY_SLAVE(fd), 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pts_ioctl: unable to map pty_seg: %s\n", e2s(r));
	return -1;
    }
    
    r = pty_ioctl(fd, req, ap, ps);
    segment_unmap_delayed(ps, 1);
    return r;
}

struct Dev devptm = {
    .dev_id = 'x',
    .dev_name = "ptm",
    .dev_read = pty_read,
    .dev_write = ptm_write,
    .dev_open = ptm_open,
    .dev_close = ptm_close,
    .dev_stat = ptm_stat,
    .dev_probe = pty_probe,
    .dev_statsync = pty_statsync,
    .dev_shutdown = pty_shutdown,
    .dev_ioctl = ptm_ioctl,
    .dev_addref = &pty_addref,
    .dev_unref = &pty_unref,
};

struct Dev devpts = {
    .dev_id = 'y',
    .dev_name = "pts",
    .dev_read = pty_read,
    .dev_write = pts_write,
    .dev_open = pts_open,
    .dev_close = pts_close,
    .dev_stat = pts_stat,
    .dev_probe = pty_probe,
    .dev_statsync = pty_statsync,
    .dev_shutdown = pty_shutdown,
    .dev_ioctl = pts_ioctl,
    .dev_addref = &pty_addref,
    .dev_unref = &pty_unref,
};
