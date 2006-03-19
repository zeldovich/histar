extern "C" {
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>

#include <termios/kernel_termios.h>
#include <bits/unimpl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <stddef.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

// Bottom of file descriptor area
#define FDTABLE		(UFDBASE)
// Return the 'struct Fd*' for file descriptor index i
#define INDEX2FD(i)	((struct Fd*) (FDTABLE + (i)*PGSIZE))

// Multiple threads with different labels could be running in the same address
// space, so it's useful to have a common place accessible by all threads to
// store this information.
static struct {
    uint64_t fd_taint;
    uint64_t fd_grant;
} fd_handles[MAXFD];
static int fd_handles_inited;

static struct {
    uint64_t valid_proc_ct;
    int mapped;
    struct cobj_ref seg;
    uint64_t flags;
} fd_map_cache[MAXFD];

static int debug = 0;


/********************************
 * FILE DESCRIPTOR MANIPULATORS *
 *                              *
 ********************************/

int
getdtablesize(void) __THROW
{
    return MAXFD;
}

int
fd2num(struct Fd *fd)
{
	return ((uintptr_t) fd - FDTABLE) / PGSIZE;
}

static int
fd_count_handles(uint64_t taint, uint64_t grant)
{
	int cnt = 0;

	for (int i = 0; i < MAXFD; i++) {
		if (fd_handles[i].fd_taint == taint)
			cnt++;
		if (fd_handles[i].fd_grant == grant)
			cnt++;
	}

	return cnt;
}

static void
fd_handles_init(void)
{
    if (fd_handles_inited)
	return;

    for (int i = 0; i < MAXFD; i++) {
	struct Fd *fd;
	int r = fd_lookup(i, &fd, 0, 0);
	if (r < 0)
	    continue;

	fd_handles[i].fd_grant = fd->fd_grant;
	fd_handles[i].fd_taint = fd->fd_taint;
    }
    fd_handles_inited = 1;
}

// Finds the smallest i from 0 to MAXFD-1 that doesn't have
// its fd page mapped.
// Sets *fd_store to the corresponding fd page virtual address.
//
// fd_alloc does NOT actually allocate an fd page.
// It is up to the caller to allocate the page somehow.
// This means that if someone calls fd_alloc twice in a row
// without allocating the first page we return, we'll return the same
// page the second time.
//
// Hint: Use INDEX2FD.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_MAX_FD: no more file descriptors
// On error, *fd_store is set to 0.

static struct cobj_ref fd_segment_cache;

int
fd_alloc(struct Fd **fd_store, const char *name)
{
    fd_handles_init();

    int i;
    struct Fd *fd;

    for (i = 0; i < MAXFD; i++) {
	fd = INDEX2FD(i);
	int r = fd_lookup(i, 0, 0, 0);
	if (r < 0)
	    break;
    }

    *fd_store = 0;
    if (i == MAXFD)
	return -E_MAX_OPEN;

    // Inherit label from container, which is { P_T:3, P_G:0, 1 }
    struct cobj_ref seg;
    int r;

    if (fd_segment_cache.container == start_env->proc_container) {
	seg = fd_segment_cache;
	fd_segment_cache.container = 0;

	uint64_t pgsize = PGSIZE;
	r = segment_map(seg, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &fd, &pgsize);
    } else {
	char nbuf[KOBJ_NAME_LEN];
	snprintf(&nbuf[0], KOBJ_NAME_LEN, "fd_alloc: %s", name);
	r = segment_alloc(start_env->proc_container, PGSIZE, &seg,
			  (void **) &fd, 0, &nbuf[0]);
    }

    if (r < 0)
	return r;

    fd_map_cache[i].valid_proc_ct = start_env->proc_container;
    fd_map_cache[i].mapped = 1;
    fd_map_cache[i].seg = seg;
    fd_map_cache[i].flags = SEGMAP_READ | SEGMAP_WRITE;

    atomic_set(&fd->fd_ref, 1);
    fd->fd_dev_id = 0;
    fd->fd_private = 1;

    *fd_store = fd;
    return 0;
}

int
fd_make_public(int fdnum)
{
    fd_handles_init();

    struct Fd *fd;
    struct cobj_ref old_seg;
    uint64_t fd_flags;
    int r = fd_lookup(fdnum, &fd, &old_seg, &fd_flags);
    if (r < 0)
	return r;

    if (fd->fd_private == 0)
	return 0;

    if (fd->fd_immutable)
	return -E_LABEL;

    int64_t fd_grant = sys_handle_create();
    if (fd_grant < 0)
	return fd_grant;
    scope_guard<void, uint64_t> grant_drop(thread_drop_star, fd_grant);

    int64_t fd_taint = sys_handle_create();
    if (fd_taint < 0)
	return fd_taint;
    scope_guard<void, uint64_t> taint_drop(thread_drop_star, fd_taint);

    label l;
    thread_cur_label(&l);
    l.transform(label::star_to, 1);
    l.set(fd_grant, 0);
    l.set(fd_taint, 3);

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(old_seg, &name[0]);
    if (r < 0)
	return r;

    int64_t new_id = sys_segment_copy(old_seg, start_env->shared_container,
				      l.to_ulabel(), &name[0]);
    if (new_id < 0)
	return new_id;

    struct cobj_ref new_seg = COBJ(start_env->shared_container, new_id);

    for (int i = 0; i < MAXFD; i++) {
	struct Fd *ifd;
	struct cobj_ref iobj;
	uint64_t iflags;

	if (fd_lookup(i, &ifd, &iobj, &iflags) < 0)
	    continue;

	if (iobj.object == old_seg.object) {
	    uint64_t pgsize = PGSIZE;
	    assert(0 == sys_segment_addref(new_seg, start_env->shared_container));
	    assert(0 == segment_unmap(ifd));
	    assert(0 == segment_map(new_seg, iflags, (void **) &ifd, &pgsize));
	    sys_obj_unref(old_seg);

	    fd_map_cache[i].mapped = 1;
	    fd_map_cache[i].valid_proc_ct = start_env->proc_container;
	    fd_map_cache[i].seg = new_seg;
	    fd_map_cache[i].flags = iflags;

	    fd_handles[i].fd_grant = fd_grant;
	    fd_handles[i].fd_taint = fd_taint;
	}
    }

    // Drop an extraneous reference
    assert(0 == sys_obj_unref(new_seg));

    fd->fd_grant = fd_grant;
    fd->fd_taint = fd_taint;
    fd->fd_private = 0;

    grant_drop.dismiss();
    taint_drop.dismiss();

    return 0;
}

// Check that fdnum is in range and mapped.
// If it is, set *fd_store to the fd page virtual address.
//
// Returns 0 on success (the page is in range and mapped), < 0 on error.
// Errors are:
//	-E_INVAL: fdnum was either not in range or not mapped.
int
fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp, uint64_t *flagsp)
{
    if (fdnum < 0 || fdnum >= MAXFD) {
	if (debug)
	    cprintf("[%lx] bad fd %d\n", thread_id(), fdnum);
	return -E_INVAL;
    }
    struct Fd *fd = INDEX2FD(fdnum);

    int r;
    struct cobj_ref seg;
    uint64_t flags;
    if (fd_map_cache[fdnum].valid_proc_ct == start_env->proc_container) {
	seg = fd_map_cache[fdnum].seg;
	flags = fd_map_cache[fdnum].flags;
	r = fd_map_cache[fdnum].mapped;
    } else {
	r = segment_lookup(fd, &seg, 0, &flags);
	if (r < 0)
	    return r;

	fd_map_cache[fdnum].mapped = r;
	fd_map_cache[fdnum].valid_proc_ct = start_env->proc_container;
	if (r > 0) {
	    fd_map_cache[fdnum].seg = seg;
	    fd_map_cache[fdnum].flags = flags;
	}
    }

    if (r == 0) {
	if (debug)
	    cprintf("[%lx] closed fd %d\n", thread_id(), fdnum);
	return -E_INVAL;
    }

    if (fd_store)
	*fd_store = fd;
    if (objp)
	*objp = seg;
    if (flagsp)
	*flagsp = flags;

    return 0;
}

// Frees file descriptor 'fd' by closing the corresponding file
// and unmapping the file descriptor page.
// Returns 0 on success, < 0 on error.
int
fd_close(struct Fd *fd)
{
    fd_handles_init();

    int r = 0;
    int handle_refs = fd_count_handles(fd->fd_taint, fd->fd_grant);

    struct cobj_ref fd_seg;
    r = segment_lookup(fd, &fd_seg, 0, 0);
    if (r < 0)
	return r;
    if (r == 0)
	return -E_NOT_FOUND;

    struct Dev *dev;
    r = dev_lookup(fd->fd_dev_id, &dev);
    if (r < 0)
	return r;

    int lastref = 0;
    if (!fd->fd_immutable && atomic_dec_and_test(&fd->fd_ref)) {
	lastref = 1;
	r = (*dev->dev_close)(fd);
    }

    if (fd->fd_private && lastref &&
	fd_segment_cache.container != start_env->proc_container)
    {
	memset(fd, 0, offsetof(struct Fd, fd_dev_state));
	fd_segment_cache = fd_seg;
    } else {
	sys_obj_unref(fd_seg);
    }
    segment_unmap_delayed(fd, 1);

    fd_map_cache[fd2num(fd)].valid_proc_ct = start_env->proc_container;
    fd_map_cache[fd2num(fd)].mapped = 0;

    if (handle_refs == 2) try {
	fd_give_up_privilege(fd2num(fd));
    } catch (std::exception &e) {
	cprintf("fd_close: cannot drop handle: %s\n", e.what());
    }

    fd_handles[fd2num(fd)].fd_taint = 0;
    fd_handles[fd2num(fd)].fd_grant = 0;

    return r;
}

void
fd_give_up_privilege(int fdnum)
{
    thread_drop_star(fd_handles[fdnum].fd_taint);
    thread_drop_star(fd_handles[fdnum].fd_grant);
}

int
fd_setflags(struct Fd *fd, struct cobj_ref fd_seg, uint64_t fd_flags)
{
    uint64_t pgsize = PGSIZE;
    int r = segment_map(fd_seg, fd_flags, (void **) &fd, &pgsize);
    if (r < 0) {
	fd_map_cache[fd2num(fd)].valid_proc_ct = 0;
	return r;
    }

    fd_map_cache[fd2num(fd)].valid_proc_ct = start_env->proc_container;
    fd_map_cache[fd2num(fd)].mapped = 1;
    fd_map_cache[fd2num(fd)].seg = fd_seg;
    fd_map_cache[fd2num(fd)].flags = fd_flags;
    return 0;
}


/******************
 * FILE FUNCTIONS *
 *                *
 ******************/

static struct Dev *devtab[] =
{
	&devcons,
	&devsock,
	&devfile,
	&devpipe,
	0
};

int
dev_lookup(int dev_id, struct Dev **dev)
{
	int i;
	for (i = 0; devtab[i]; i++)
		if (devtab[i]->dev_id == dev_id) {
			*dev = devtab[i];
			return 0;
		}
	cprintf("[%lx] unknown device type %d\n", thread_id(), dev_id);
	*dev = 0;
	return -E_INVAL;
}

int
close(int fdnum) __THROW
{
	struct Fd *fd;
	int r;

	if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0)
		return r;
	else
		return fd_close(fd);
}

void
close_all(void)
{
	int i;
	for (i = 0; i < MAXFD; i++)
		close(i);
}

// Make file descriptor 'newfdnum' a duplicate of file descriptor 'oldfdnum'.
// For instance, writing onto either file descriptor will affect the
// file and the file offset of the other.
// Closes any previously open file descriptor at 'newfdnum'.
// This is implemented using virtual memory tricks (of course!).
int
dup2(int oldfdnum, int newfdnum) __THROW
{
    struct Fd *oldfd;
    struct cobj_ref fd_seg;
    uint64_t fd_flags;
    int r = fd_lookup(oldfdnum, &oldfd, &fd_seg, &fd_flags);
    if (r < 0) {
	__set_errno(EBADF);
	return -1;
    }

    r = sys_segment_addref(fd_seg, fd_seg.container);
    if (r < 0) {
	__set_errno(EPERM);
	return -1;
    }

    close(newfdnum);
    struct Fd *newfd = INDEX2FD(newfdnum);

    int immutable = oldfd->fd_immutable;
    uint64_t pgsize = PGSIZE;
    fd_flags &= ~SEGMAP_CLOEXEC;
    r = segment_map(fd_seg, fd_flags,
		    (void**) &newfd, &pgsize);
    if (r < 0) {
	fd_map_cache[newfdnum].valid_proc_ct = 0;
	sys_obj_unref(fd_seg);
	__set_errno(EINVAL);
	return -1;
    }

    fd_map_cache[newfdnum].mapped = 1;
    fd_map_cache[newfdnum].valid_proc_ct = start_env->proc_container;
    fd_map_cache[newfdnum].seg = fd_seg;
    fd_map_cache[newfdnum].flags = fd_flags;

    if (!immutable)
	atomic_inc(&oldfd->fd_ref);

    fd_handles[newfdnum].fd_taint = oldfd->fd_taint;
    fd_handles[newfdnum].fd_grant = oldfd->fd_grant;

    return newfdnum;
}

int
dup(int fdnum) __THROW
{
    for (int i = 0; i < MAXFD; i++) {
	int r = fd_lookup(i, 0, 0, 0);
	if (r < 0)
	    return dup2(fdnum, i);
    }

    __set_errno(EMFILE);
    return -1;
}

int
dup2_as(int oldfdnum, int newfdnum, struct cobj_ref target_as, uint64_t target_ct)
{
    int r = fd_make_public(oldfdnum);
    if (r < 0) {
	cprintf("dup2_as: make_public: %s\n", e2s(r));
	return r;
    }

    struct Fd *oldfd;
    struct cobj_ref old_seg;
    uint64_t fd_flags;
    r = fd_lookup(oldfdnum, &oldfd, &old_seg, &fd_flags);
    if (r < 0) {
	cprintf("dup2_as: fd_lookup: %s\n", e2s(r));
	return r;
    }

    if (old_seg.container != start_env->shared_container &&
	old_seg.container != start_env->proc_container)
    {
	cprintf("dup2_as: strange container %ld (shared %ld proc %ld)\n",
		old_seg.container,
		start_env->shared_container,
		start_env->proc_container);
    }

    r = sys_segment_addref(old_seg, target_ct);
    if (r < 0) {
	cprintf("dup2_as: sys_segment_addref: %s\n", e2s(r));
	return r;
    }

    struct cobj_ref new_seg = COBJ(target_ct, old_seg.object);

    // XXX only works for initial setup, as this doesn't close
    // newfdnum in target address space if one aleady exists.
    struct Fd *newfd = INDEX2FD(newfdnum);

    int immutable = oldfd->fd_immutable;
    fd_flags &= ~SEGMAP_CLOEXEC;
    if (immutable)
	fd_flags &= ~SEGMAP_WRITE;

    r = segment_map_as(target_as, new_seg, fd_flags,
		       (void**) &newfd, 0);
    if (r < 0) {
	cprintf("dup2_as: segment_map_as: %s\n", e2s(r));
	sys_obj_unref(new_seg);
	return r;
    }

    if (!immutable)
	atomic_inc(&oldfd->fd_ref);
    return newfdnum;
}

ssize_t
read(int fdnum, void *buf, size_t n) __THROW
{
	int64_t r;
	struct Dev *dev;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	    || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;
	if ((fd->fd_omode & O_ACCMODE) == O_WRONLY) {
		cprintf("[%lx] read %d -- bad mode\n", thread_id(), fdnum); 
		return -E_INVAL;
	}
	r = (*dev->dev_read)(fd, buf, n, fd->fd_offset);
	if (r >= 0 && !fd->fd_immutable)
		fd->fd_offset += r;
	return r;
}

ssize_t
readn(int fdnum, void *buf, size_t n)
{
	size_t tot;
	int64_t m;

	for (tot = 0; tot < n; tot += m) {
		m = read(fdnum, (char*)buf + tot, n - tot);
		if (m < 0)
			return m;
		if (m == 0)
			break;
	}
	return tot;
}

ssize_t
write(int fdnum, const void *buf, size_t n) __THROW
{
	int64_t r;
	struct Dev *dev;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	    || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		return r;
	if ((fd->fd_omode & O_ACCMODE) == O_RDONLY) {
		cprintf("[%lx] write %d -- bad mode\n", thread_id(), fdnum);
		return -E_INVAL;
	}
	if (debug)
		cprintf("write %d %p %ld via dev %s\n",
			fdnum, buf, n, dev->dev_name);
	r = (*dev->dev_write)(fd, buf, n, fd->fd_offset);
	if (r > 0 && !fd->fd_immutable)
		fd->fd_offset += r;
	return r;
}

int
bind(int fdnum, const struct sockaddr *addr, socklen_t addrlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_bind(fd, addr, addrlen);
}

int
connect(int fdnum, const struct sockaddr *addr, socklen_t addrlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_connect(fd, addr, addrlen);
}

int
listen(int fdnum, int backlog) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_listen(fd, backlog);
}

int
accept(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_accept(fd, addr, addrlen);
}

int 
getsockname(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
        return r;

    return dev->dev_getsockname(fd, addr, addrlen);
}

int 
getpeername(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
        return r;

    return dev->dev_getpeername(fd, addr, addrlen);   
}


int 
setsockopt(int fdnum, int level, int optname, const void *optval, 
           socklen_t optlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
        return r;

    return dev->dev_setsockopt(fd, level, optname, optval, optlen);       
}
               
int 
getsockopt(int fdnum, int level, int optname,void *optval, 
           socklen_t *optlen) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
        return r;

    return dev->dev_getsockopt(fd, level, optname, optval, optlen);    
}

// XXX
int
select(int maxfd, fd_set *readset, fd_set *writeset, fd_set *exceptset,
       struct timeval *timeout) __THROW
{
    // XXX
    if (exceptset)
        FD_ZERO(exceptset) ;
    
    fd_set rreadset ;
    fd_set rwriteset ;
    FD_ZERO(&rreadset) ;
    FD_ZERO(&rwriteset) ;
    
    int ready = 0 ;
    
    struct Fd *fd;
    struct Dev *dev;
    int r ;
    
    struct timeval start ;
    gettimeofday(&start, 0) ;
    
    while (1) {
        for (int i = 0 ; i < maxfd ; i++) {
            if (readset && FD_ISSET(i, readset)) {
                if ((r = fd_lookup(i, &fd, 0, 0)) < 0
                || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
                    __set_errno(EBADF);
                    return -1 ;
                }
                if (dev->dev_probe(fd, dev_probe_read)) {
                    FD_SET(i, &rreadset) ;
                    ready++ ;    
                }
            }
            if (writeset && FD_ISSET(i, writeset)) {
                if ((r = fd_lookup(i, &fd, 0, 0)) < 0
                || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0) {
                    __set_errno(EBADF);
                    return -1 ;
                }
                if (dev->dev_probe(fd, dev_probe_write)) {
                    FD_SET(i, &rwriteset) ;
                    ready++ ;    
                }
            }
        }
        // XXX can exceed timeout...
        if (timeout) {
            struct timeval now, elapsed ;
            gettimeofday(&now, 0) ;
            timeradd(&start, &now, &elapsed) ;
            if (timercmp(&elapsed, timeout, >))
                break ;
        }
        if (!ready)
            usleep(100000) ;
        else
            break ;
    }
    
    if (writeset)
        memcpy(writeset, &rwriteset, sizeof(*writeset)) ;
    if (readset)
        memcpy(readset, &rreadset, sizeof(*readset)) ;
    return ready ;
}

ssize_t
send(int fdnum, const void *dataptr, size_t size, int flags) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
    {
	__set_errno(EBADF);
	return -1;
    }

    if (dev->dev_send == 0) {
	__set_errno(EOPNOTSUPP);
	return -1;
    }

    return dev->dev_send(fd, dataptr, size, flags);
}

ssize_t
recv(int fdnum, void *mem, size_t len, int flags) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
    {
	__set_errno(EBADF);
	return -1;
    }

    if (dev->dev_recv == 0) {
	__set_errno(EOPNOTSUPP);
	return -1;
    }

    return dev->dev_recv(fd, mem, len, flags);
}

int
fstat(int fdnum, struct stat *buf) __THROW
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
    {
	cprintf("fstat(%d): %s\n", fdnum, e2s(r));
	__set_errno(EBADF);
	return -1;
    }

    if (dev->dev_stat == 0) {
	__set_errno(EOPNOTSUPP);
	return -1;
    }

    return dev->dev_stat(fd, buf);
}

extern "C" int
__libc_fcntl(int fdnum, int cmd, ...) __THROW
{
    int r;
    va_list ap;
    long arg = 0 ;
    struct flock *flock = 0 ;
    struct Fd *fd;

    struct cobj_ref fd_obj;
    uint64_t fd_flags;

    if ((r = fd_lookup(fdnum, &fd, &fd_obj, &fd_flags)) < 0) {
	__set_errno(EBADF);
	return -1;
    }

    va_start(ap, cmd);
    if (cmd == F_DUPFD || cmd == F_GETFD || cmd == F_SETFD) {
	arg = va_arg(ap, long);
    } else if (cmd == F_GETFL || cmd == F_SETFL) { 
	arg = va_arg(ap, long);
    } else if (cmd == F_GETLK || cmd == F_SETLK || cmd == F_SETLKW) {
	flock = va_arg(ap, struct flock *);
    }
    va_end(ap);

    if (cmd == F_DUPFD) {
    	for (int i = arg; i < MAXFD; i++) {
    	    r = fd_lookup(i, 0, 0, 0);
    	    if (r < 0)
		return dup2(fdnum, i);
    	}
    
    	__set_errno(EMFILE);
    	return -1;
    }

    if (cmd == F_GETFD)
	return (fd_flags & SEGMAP_CLOEXEC) ? FD_CLOEXEC : 0;

    if (cmd == F_SETFD) {
	fd_flags &= ~SEGMAP_CLOEXEC;
	if ((arg & FD_CLOEXEC))
	    fd_flags |= SEGMAP_CLOEXEC;

	r = fd_setflags(fd, fd_obj, fd_flags);
	if (r < 0) {
	    __set_errno(EINVAL);
	    return -1;
	}

	return 0;
    }

    if (cmd == F_SETFL) {
	if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0)
	    return r;
        
	int mask = (O_APPEND | O_ASYNC | O_DIRECT | O_NOATIME | O_NONBLOCK);
	fd->fd_omode &= ~mask;
	fd->fd_omode |= (arg & mask);
	return 0;
    }

    if (cmd == F_GETFL) {
        return fd->fd_omode;
    }

    cprintf("Unimplemented fcntl call: %d\n", cmd);
    __set_errno(ENOSYS);
    return -1;
}

off_t
lseek(int fdnum, off_t offset, int whence) __THROW
{
    return lseek64(fdnum, offset, whence);
}

__off64_t
lseek64(int fdnum, __off64_t offset, int whence) __THROW
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0) {
	__set_errno(EBADF);
	return -1;
    }

    if (fd->fd_immutable) {
	__set_errno(EINVAL);
	return -1;
    }

    if (whence == SEEK_SET) {
	fd->fd_offset = offset;
    } else if (whence == SEEK_CUR) {
	fd->fd_offset += offset;
    } else if (whence == SEEK_END) {
	struct stat st;
	if (fstat(fdnum, &st) < 0)
	    return -1;

	fd->fd_offset = st.st_size + offset;
    } else {
	__set_errno(EINVAL);
	return -1;
    }

    return 0;
}

int
flock(int fd, int operation) __THROW
{
    set_enosys();
    return -1;
}

int
fchown(int fd, uid_t owner, gid_t group) __THROW
{
    set_enosys();
    return -1;
}

int
fd_set_isatty(int fdnum, int isit)
{
    int r;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0)
	return r;

    fd->fd_isatty = isit;
    return 0;
}

int
ioctl(int fdnum, unsigned long int req, ...) __THROW
{
    int r;
    va_list ap;
    struct Fd *fd;
    struct __kernel_termios *k_termios;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0) {
    	__set_errno(EBADF);
    	return -1;
    }

    va_start(ap, req);
    if (req == TCGETS)
	k_termios = va_arg(ap, struct __kernel_termios *);
    va_end(ap);

    if (req == TCGETS) {
    	if (!fd->fd_isatty) {
	    __set_errno(ENOTTY);
	    return -1;
    	}

	if (k_termios)
	    memset(k_termios, 0, sizeof(*k_termios));

	return 0;
    }

    if (req == TCSETS) {
        if (!fd->fd_isatty) {
	    __set_errno(ENOTTY);
            return -1;
        }

        //cprintf("ioctl: TCSETS: not actually setting termios\n") ;  
    }

    __set_errno(EINVAL);
    return -1;
}

extern "C" ssize_t
__getdents (int fdnum, struct dirent *buf, size_t nbytes)
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
    {
	__set_errno(EBADF);
	return -1;
    }

    if (dev->dev_getdents == 0) {
	__set_errno(EOPNOTSUPP);
	return -1;
    }

    return dev->dev_getdents(fd, buf, nbytes);
}

weak_alias(__libc_fcntl, fcntl);
