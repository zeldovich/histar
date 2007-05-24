#include <inc/lib.h>
#include <inc/bipipe.h>
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
    
    uint64_t ct = start_env->shared_container;
    struct bipipe_seg *bs = 0;
    struct cobj_ref bipipe_seg;
    if ((r = segment_alloc(ct, sizeof(*bs), &bipipe_seg, 
			   (void **)&bs, &label, "pty-bipipe")) < 0) {
	jos_fd_close(fd);
	errno = ENOMEM;
	return -1;        
    }
    memset(bs, 0, sizeof(*bs));
   
    struct pty_seg *ps = 0;
    struct cobj_ref slave_pty_seg;
    if ((r = segment_alloc(ct, sizeof(*ps), &slave_pty_seg, 
			   (void **)&ps, &label, "pty-slave-ios")) < 0) {
	jos_fd_close(fd);
	segment_unmap(bs);
	errno = ENOMEM;
	return -1;        
    }
    memset(ps, 0, sizeof(*ps));
    ps->master_open = 1;

    fd->fd_ptm.bipipe_seg = bipipe_seg;
    fd->fd_ptm.slave_pty_seg = slave_pty_seg;

    fd->fd_ptm.bipipe_a = 1;
    bs->p[1].open = 1;    

    segment_unmap_delayed(bs, 1);
    segment_unmap_delayed(ps, 1);

    struct pts_descriptor pd;
    pd.slave_pty_seg = slave_pty_seg;
    pd.slave_bipipe_seg = bipipe_seg;
    pd.grant = grant;
    pd.taint = taint;
    r = pty_alloc(&pd);
    if (r < 0) {
	jos_fd_close(fd);
	__set_errno(ENOTSUP);
	return -1;
    }
    fd->fd_ptm.ptyno = r;
    
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
	__set_errno(ENOTSUP);
	return -1;
    }
    
    fd->fd_pts.ptyno = ptyno;
    fd->fd_pts.bipipe_seg = pd.slave_bipipe_seg;
    fd->fd_pts.pty_seg = pd.slave_pty_seg;
    fd->fd_pts.bipipe_a = 0;
    fd_set_extra_handles(fd, pd.taint, pd.grant);
    
    struct bipipe_seg *bs = 0;
    r = segment_map(fd->fd_pts.bipipe_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **)&bs, 0, 0);
    if (r < 0) {
	/* common for processes (via libc) to open pty slaves they don't
	 * have the priv. for.  Note, we are closing a 'parially' allocated 
	 * fd.
	 * cprintf("pts_open: unable to map bipipe_seg: %s\n", e2s(r));
	 */
	errno = EACCES;
	jos_fd_close(fd);
	return -1;
    }
    bs->p[0].open = 1;    
    segment_unmap_delayed(bs, 1);

    struct pty_seg *ps = 0;
    r = segment_map(fd->fd_pts.pty_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
		    (void **)&ps, 0, 0);
    if (r < 0) {
	cprintf("pts_open: unable to map pty_seg: %s\n", e2s(r));
	jos_fd_close(fd);
	return -1;
    }

    /* A number of different fds can point to the same slave pty device,
     * while each pty master fd points to a different slave pty device.
     */
    ps->slave_ref++;
    segment_unmap_delayed(ps, 1);
    return fd2num(fd);
}

static int
pts_close(struct Fd *fd)
{
    // a 'partially' allocated pts
    if (!fd->fd_pts.pty_seg.object) 
	return 0;

    struct pty_seg *ps = 0;
    int r = segment_map(fd->fd_pts.pty_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **)&ps, 0, 0);
    if (r < 0) {
	struct cobj_ref obj = fd->fd_ptm.slave_pty_seg;
	cprintf("pts_close: unable to map slave_pty_seg (%"PRIu64", %"PRIu64"): %s\n", 
		obj.container, obj.object, e2s(r));
	return -1;
    }

    jthread_mutex_lock(&ps->mu);
    
    if (!ps->master_open) {
	r = sys_obj_unref(fd->fd_pts.pty_seg);
	if (r < 0)
	    cprintf("pts_close: can't unref pty_seg: %s\n", e2s(r));
    }
    
    ps->slave_ref--;
    if (ps->slave_ref == 0)
	r = (*devbipipe.dev_close)(fd);
    else
	r = 0;

    jthread_mutex_unlock(&ps->mu);

    segment_unmap(ps);
    return r;
}

static int
ptm_close(struct Fd *fd)
{
    pty_remove(fd->fd_ptm.ptyno);

    struct pty_seg *ps = 0;
    int r = segment_map(fd->fd_ptm.slave_pty_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **)&ps, 0, 0);
    if (r < 0) {
	struct cobj_ref obj = fd->fd_ptm.slave_pty_seg;
	cprintf("ptm_close: unable to map slave_pty_seg (%"PRIu64", %"PRIu64"): %s\n", 
		obj.container, obj.object, e2s(r));
	return -1;
    }
    
    jthread_mutex_lock(&ps->mu);

    ps->master_open = 0;
    if (!ps->slave_ref) {
	r = sys_obj_unref(fd->fd_ptm.slave_pty_seg);
	if (r < 0)
	    cprintf("ptm_close: can't unref slave_pty_seg: %s\n", e2s(r));
    }

    jthread_mutex_unlock(&ps->mu);
    segment_unmap(ps);
    return (*devbipipe.dev_close)(fd);
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
    
    int r = (*devbipipe.dev_write)(fd, bf, cc, 0); 
    if (r < 0)
	return r;

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
	int rr = (*devbipipe.dev_write)(fd, bf, cc - r, r); 
	if (rr < 0) {
	    cprintf("pty_write: error on bipipe write: %s\n", e2s(rr));
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
    return pty_write(fd, buf, count, &fd->fd_ptm.ps);
}

static ssize_t
pts_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    struct pty_seg *ps = 0;
    int r = segment_map(fd->fd_pts.pty_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
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
    return (*devbipipe.dev_read)(fd, buf, count, offset);
}

static int
pty_probe(struct Fd *fd, dev_probe_t probe)
{
    return (*devbipipe.dev_probe)(fd, probe);
}

static int
pty_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{
    return  (*devbipipe.dev_statsync)(fd, probe, wstat);
}

static int
pts_stat(struct Fd *fd, struct stat *buf)
{
    buf->st_mode |= S_IFCHR;
    buf->st_rdev = fd->fd_pts.ptyno;
    return 0;
}

static int
ptm_stat(struct Fd *fd, struct stat *buf)
{
    buf->st_mode |= S_IFCHR;
    return 0;
}

static int
pty_shutdown(struct Fd *fd, int how)
{
    return (*devbipipe.dev_shutdown)(fd, how);
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
	*ptyno = fd->fd_ptm.ptyno;
	return 0;
    } if (req == TIOCSPTLCK) {
	// the pts associated with fd is always unlocked
	return 0;
    } 

    return pty_ioctl(fd, req, ap, &fd->fd_ptm.ps);
}

static int
pts_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    struct pty_seg *ps = 0;
    int r = segment_map(fd->fd_pts.pty_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
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
};
