extern "C" {
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/pt.h>
#include <inc/sigio.h>

#include <termios/kernel_termios.h>
#include <bits/unimpl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/statfs.h>
#include <sys/poll.h>

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

extern uint64_t signal_counter;

// Bottom of file descriptor area
#define FDTABLE		(UFDBASE)
// Return the 'struct Fd*' for file descriptor index i
#define INDEX2FD(i)	((struct Fd*) (FDTABLE + (i)*PGSIZE))

enum { fd_missing_debug = 1 };
enum { fd_handle_debug = 0 };
enum { fd_alloc_debug = 0 };

// Check for null function pointers before invoking a device method
#define DEV_CALL(dev, fn, ...)					\
    ({								\
	__typeof__(dev->dev_##fn (__VA_ARGS__)) __r;		\
	if (!dev->dev_##fn) {					\
	    if (fd_missing_debug)				\
		cprintf("Missing op: %s for type %s\n",		\
			#fn, dev->dev_name);			\
	    __set_errno(EOPNOTSUPP);				\
	    __r = -1;						\
	} else {						\
	    __r = dev->dev_##fn (__VA_ARGS__);			\
	}							\
	__r;							\
    })

// Call a method on a file descriptor number
#define FD_CALL(fdnum, fn, ...)					\
    ({								\
	struct Fd *__fd;					\
	struct Dev *__dev;					\
	if (fd_lookup(fdnum, &__fd, 0, 0) < 0 ||		\
	    dev_lookup(__fd->fd_dev_id, &__dev) < 0)		\
	{							\
	    __set_errno(EBADF);					\
	    return -1;						\
	}							\
	if (__fd->fd_omode & O_ASYNC)				\
	    jos_sigio_activate(fdnum);				\
	DEV_CALL(__dev, fn, __fd, ##__VA_ARGS__);		\
    })

// Multiple threads with different labels could be running in the same address
// space, so it's useful to have a common place accessible by all threads to
// store this information.
static struct {
    uint64_t h[fd_handle_max];
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
fd_count_handles(uint64_t h)
{
    int cnt = 0;

    for (int i = 0; i < MAXFD; i++)
	for (int j = 0; j < fd_handle_max; j++)
	    if (fd_handles[i].h[j] == h)
		cnt++;

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

	for (int j = 0; j < fd_handle_max; j++)
	    fd_handles[i].h[j] = fd->fd_handle[j];
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
    static_assert(sizeof(*fd) <= PGSIZE);

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
	r = segment_map(seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &fd, &pgsize, 0);
    } else {
	char nbuf[KOBJ_NAME_LEN];
	snprintf(&nbuf[0], KOBJ_NAME_LEN, "fd_alloc: %s", name);
	r = segment_alloc(start_env->proc_container, PGSIZE, &seg,
			  (void **) &fd, 0, &nbuf[0]);

	// Finalize the quota of the segment so it can be hard-linked
	if (r >= 0)
	    assert(0 == sys_obj_set_fixedquota(seg));
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

    if (fd_alloc_debug)
	cprintf("[%ld] fd_alloc: fd %d (%s)\n", thread_id(), fd2num(fd), name);

    return 0;
}

int
fd_make_public(int fdnum, struct ulabel *ul_taint)
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

    int64_t fd_grant = handle_alloc();
    if (fd_grant < 0)
	return fd_grant;
    scope_guard<void, uint64_t> grant_drop(thread_drop_star, fd_grant);

    int64_t fd_taint = handle_alloc();
    if (fd_taint < 0)
	return fd_taint;
    scope_guard<void, uint64_t> taint_drop(thread_drop_star, fd_taint);

    label l;
    thread_cur_label(&l);
    l.transform(label::star_to, 1);
    l.set(fd_grant, 0);
    l.set(fd_taint, 3);

    if (ul_taint) {
	label taint;
	taint.copy_from(ul_taint);

	label out;
	l.merge(&taint, &out, label::max, label::leq_starlo);
	l.copy_from(&out);
    }

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(old_seg, &name[0]);
    if (r < 0)
	return r;

    int64_t new_id = sys_segment_copy(old_seg, start_env->shared_container,
				      l.to_ulabel(), &name[0]);
    if (new_id < 0)
	return new_id;

    // finalize for hard-linking
    struct cobj_ref new_seg = COBJ(start_env->shared_container, new_id);
    assert(0 == sys_obj_set_fixedquota(new_seg));

    for (int i = 0; i < MAXFD; i++) {
	struct Fd *ifd;
	struct cobj_ref iobj;
	uint64_t iflags;

	if (fd_lookup(i, &ifd, &iobj, &iflags) < 0)
	    continue;

	if (iobj.object == old_seg.object) {
	    uint64_t pgsize = PGSIZE;
	    assert(0 == sys_segment_addref(new_seg, start_env->shared_container));
	    assert(0 == segment_map(new_seg, 0, iflags, (void **) &ifd, &pgsize, SEG_MAPOPT_REPLACE));
	    sys_obj_unref(old_seg);

	    fd_map_cache[i].mapped = 1;
	    fd_map_cache[i].valid_proc_ct = start_env->proc_container;
	    fd_map_cache[i].seg = new_seg;
	    fd_map_cache[i].flags = iflags;

	    fd_handles[i].h[fd_handle_grant] = fd_grant;
	    fd_handles[i].h[fd_handle_taint] = fd_taint;
	}
    }

    // Drop an extraneous reference
    assert(0 == sys_obj_unref(new_seg));

    fd->fd_handle[fd_handle_grant] = fd_grant;
    fd->fd_handle[fd_handle_taint] = fd_taint;
    fd->fd_private = 0;

    if (fd_handle_debug)
	cprintf("[%ld] fd_make_public(%d): grant %ld, taint %ld\n",
		thread_id(), fdnum, fd_grant, fd_taint);

    grant_drop.dismiss();
    taint_drop.dismiss();

    return 0;
}

void
fd_set_extra_handles(struct Fd *fd, uint64_t eg, uint64_t et)
{
    int fdnum = fd2num(fd);

    fd->fd_handle[fd_handle_extra_grant] = eg;
    fd->fd_handle[fd_handle_extra_taint] = et;
    fd_handles[fdnum].h[fd_handle_extra_grant] = eg;
    fd_handles[fdnum].h[fd_handle_extra_taint] = et;

    if (fd_handle_debug)
	cprintf("[%ld] fd_set_extra_handles(%d): extra grant %ld, extra taint %ld\n",
		thread_id(), fdnum, eg, et);
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
	struct u_segment_mapping usm;
	r = segment_lookup(fd, &usm);
	if (r < 0)
	    return r;

	seg = usm.segment;
	flags = usm.flags;

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
jos_fd_close(struct Fd *fd)
{
    int fdnum = fd2num(fd);
    fd_handles_init();

    int r = 0;
    int handle_refs[fd_handle_max];
    for (int i = 0; i < fd_handle_max; i++) {
	assert(fd->fd_handle[i] == fd_handles[fdnum].h[i]);
	handle_refs[i] = fd_count_handles(fd->fd_handle[i]);

	if (fd_handle_debug && fd->fd_handle[i] && handle_refs[i] > 1)
	    cprintf("[%ld] jos_fd_close(%d): refcount on handle %ld is %d\n",
		    thread_id(), fdnum, fd_handles[fdnum].h[i], handle_refs[i]);
    }

    struct u_segment_mapping usm;
    r = segment_lookup(fd, &usm);
    if (r < 0)
	return r;
    if (r == 0)
	return -E_NOT_FOUND;

    struct cobj_ref fd_seg = usm.segment;

    struct Dev *dev;
    r = dev_lookup(fd->fd_dev_id, &dev);
    if (r < 0)
	return r;

    if (fd->fd_omode & O_ASYNC)
	jos_sigio_disable(fdnum);

    int lastref = 0;
    if (!fd->fd_immutable && atomic_dec_and_test(&fd->fd_ref)) {
	lastref = 1;
	r = DEV_CALL(dev, close, fd);
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

    fd_map_cache[fdnum].valid_proc_ct = start_env->proc_container;
    fd_map_cache[fdnum].mapped = 0;

    for (int i = 0; i < fd_handle_max; i++) {
	if (handle_refs[i] == 1) try {
	    thread_drop_star(fd_handles[fdnum].h[i]);
	} catch (std::exception &e) {
	    cprintf("fd_close: cannot drop handle: %s\n", e.what());
	}

	fd_handles[fdnum].h[i] = 0;
    }

    if (fd_alloc_debug)
	cprintf("[%ld] jos_fd_close(%d)\n", thread_id(), fdnum);

    return r;
}

void
fd_give_up_privilege(int fdnum)
{
    thread_drop_starpair(fd_handles[fdnum].h[fd_handle_taint],
			 fd_handles[fdnum].h[fd_handle_grant]);
    thread_drop_starpair(fd_handles[fdnum].h[fd_handle_extra_taint],
			 fd_handles[fdnum].h[fd_handle_extra_grant]);
}

int
fd_setflags(struct Fd *fd, struct cobj_ref fd_seg, uint64_t fd_flags)
{
    uint64_t pgsize = PGSIZE;
    int r = segment_map(fd_seg, 0, fd_flags, (void **) &fd, &pgsize, SEG_MAPOPT_REPLACE);
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
 ******************/

static struct Dev *devtab[] =
{
    &devcons,
    &devsock,
    &devfile,
    &devpipe,
    &devtun,
    &devbipipe,
    &devrand,
    &devzero,
    &devnull,
    &devpt,
    0
};

int
dev_lookup(int dev_id, struct Dev **dev)
{
    int i;
    for (i = 0; devtab[i]; i++) {
	if (devtab[i]->dev_id == dev_id) {
	    *dev = devtab[i];
	    return 0;
	}
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

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0) {
	__set_errno(EBADF);
	return -1;
    } else {
	if (fd_alloc_debug)
	    cprintf("[%ld] close(%d)\n", thread_id(), fdnum);

	return jos_fd_close(fd);
    }
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
    r = segment_map(fd_seg, 0, fd_flags,
		    (void**) &newfd, &pgsize, 0);
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

    for (int i = 0; i < fd_handle_max; i++)
	fd_handles[newfdnum].h[i] = oldfd->fd_handle[i];

    if (fd_handle_debug || fd_alloc_debug)
	cprintf("[%ld] dup2: %d -> %d\n", thread_id(), oldfdnum, newfdnum);

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
    int r = fd_make_public(oldfdnum, 0);
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

    r = segment_map_as(target_as, new_seg, 0, fd_flags,
		       (void**) &newfd, 0, 0);
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
    {
	__set_errno(EBADF);
	return -1;
    }

    if ((fd->fd_omode & O_ACCMODE) == O_WRONLY) {
	cprintf("[%lx] read %d -- bad mode\n", thread_id(), fdnum); 
	__set_errno(EINVAL);
	return -1;
    }

    r = DEV_CALL(dev, read, fd, buf, n, fd->fd_offset);
    if (r >= 0 && !fd->fd_immutable)
	fd->fd_offset += r;
    if (fd->fd_omode & O_ASYNC)
	jos_sigio_activate(fdnum);
    return r;
}

ssize_t
write(int fdnum, const void *buf, size_t n) __THROW
{
    int64_t r;
    struct Dev *dev;
    struct Fd *fd;

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
    {
	__set_errno(EBADF);
	return -1;
    }

    if ((fd->fd_omode & O_ACCMODE) == O_RDONLY) {
	cprintf("[%lx] write %d -- bad mode\n", thread_id(), fdnum);
	__set_errno(EINVAL);
	return -1;
    }

    if (debug)
	cprintf("write %d %p %ld via dev %s\n",
		fdnum, buf, n, dev->dev_name);

    r = DEV_CALL(dev, write, fd, buf, n, fd->fd_offset);
    if (r > 0 && !fd->fd_immutable)
	fd->fd_offset += r;
    return r;
}

ssize_t 
readv(int fd, const struct iovec *vector, int count) __THROW
{
    int ret = 0;
    for (int i = 0; i < count; i++) {
	int r = read(fd, vector[i].iov_base, vector[i].iov_len);
	if (r < 0) {
	    if (i == 0)
		return r;
	    printf("readv: read error: %s\n", e2s(r));
	    return ret;
	}
	ret += r;
	if ((uint32_t)r < vector[i].iov_len)
	    return ret;
    }
    return ret;
}

ssize_t 
writev(int fd, const struct iovec *vector, int count) __THROW
{
    int ret = 0;
    for (int i = 0; i < count; i++) {
	int r = write(fd, vector[i].iov_base, vector[i].iov_len);
	if (r < 0) {
	    if (i == 0)
		return r;
	    printf("writev: write error: %s\n", e2s(r));
	    return ret;
	}
	ret += r;
	if ((uint32_t)r < vector[i].iov_len)
	    return ret;
    }
    return ret;
}

int
bind(int fdnum, const struct sockaddr *addr, socklen_t addrlen) __THROW
{
    return FD_CALL(fdnum, bind, addr, addrlen);
}

int
connect(int fdnum, const struct sockaddr *addr, socklen_t addrlen) __THROW
{
    return FD_CALL(fdnum, connect, addr, addrlen);
}

int
listen(int fdnum, int backlog) __THROW
{
    return FD_CALL(fdnum, listen, backlog);
}

int
accept(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    return FD_CALL(fdnum, accept, addr, addrlen);
}

int 
getsockname(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    return FD_CALL(fdnum, getsockname, addr, addrlen);
}

int 
getpeername(int fdnum, struct sockaddr *addr, socklen_t *addrlen) __THROW
{
    return FD_CALL(fdnum, getpeername, addr, addrlen);
}


int 
setsockopt(int fdnum, int level, int optname, const void *optval, 
           socklen_t optlen) __THROW
{
    return FD_CALL(fdnum, setsockopt, level, optname, optval, optlen);
}
               
int 
getsockopt(int fdnum, int level, int optname, void *optval,
           socklen_t *optlen) __THROW
{
    return FD_CALL(fdnum, getsockopt, level, optname, optval, optlen);
}

// XXX
// one issue is selecting on fd_set with size < sizeof(fd_set)...
// this can happen with fd_set *set = malloc(x).  Use FD_* macros
// carefully on arguments, and shouldn't use FD_ZERO on readset, 
// writeset or execept set.
int
select(int maxfd, fd_set *readset, fd_set *writeset, fd_set *exceptset,
       struct timeval *timeout) __THROW
{
    struct wait_stat wstat[(2 * maxfd) + 1];
    uint64_t wstat_count = 0;
    memset(wstat, 0, sizeof(wstat));
    
    uint64_t start_signal_counter = signal_counter;
    WS_SETADDR(&wstat[wstat_count], &signal_counter);
    WS_SETVAL(&wstat[wstat_count], signal_counter);
    wstat_count++;
    
    // XXX
    if (exceptset)
	for (int i = 0 ; exceptset && i < maxfd ; i++)
	    FD_CLR(i, exceptset);
    
    fd_set rreadset;
    fd_set rwriteset;
    FD_ZERO(&rreadset);
    FD_ZERO(&rwriteset);
    
    int ready = 0;
    
    struct Fd *fd;
    struct Dev *dev;
    int r;

    struct timeval start;
    gettimeofday(&start, 0);

    while (1) {
        for (int i = 0 ; i < maxfd ; i++) {
            if (readset && FD_ISSET(i, readset)) {
                if ((r = fd_lookup(i, &fd, 0, 0)) < 0
		    || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		{
                    __set_errno(EBADF);
                    return -1;
                }
		
		r = DEV_CALL(dev, statsync, fd, dev_probe_read, 
			     &wstat[wstat_count]);
		if (r == 0) {
		    wstat[wstat_count].ws_probe = dev_probe_read;
		    wstat_count++;
		}

		if (DEV_CALL(dev, probe, fd, dev_probe_read)) {
                    FD_SET(i, &rreadset);
                    ready++;
		}
            }
            if (writeset && FD_ISSET(i, writeset)) {
                if ((r = fd_lookup(i, &fd, 0, 0)) < 0
		    || (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
		{
                    __set_errno(EBADF);
                    return -1;
                }

		r = DEV_CALL(dev, statsync, fd, dev_probe_write, 
			     &wstat[wstat_count]);
		if (r == 0) {
		    wstat[wstat_count].ws_probe = dev_probe_write;
		    wstat_count++;
		}
		
		if (DEV_CALL(dev, probe, fd, dev_probe_write)) {
                    FD_SET(i, &rwriteset);
                    ready++;
		}
            }
        }
	struct timeval remaining;
        if (timeout) {
            struct timeval now, elapsed;
            gettimeofday(&now, 0);
            timersub(&now, &start, &elapsed);
            if (timercmp(&elapsed, timeout, >))
                break;
	    timersub(timeout, &elapsed, &remaining);
        }

        if (!ready) {
	    uint64_t msec = ~0UL;
	    if (timeout) {
	    	uint64_t time = sys_clock_msec();
	    	msec = time + (remaining.tv_sec * 1000) + (remaining.tv_usec / 1000);
	    }

	    multisync_wait(wstat, wstat_count, msec);
	    if (start_signal_counter != signal_counter) {
		__set_errno(EINTR);
		return -1;
	    }
	} else
            break;
	// save signal_counter
	wstat_count = 1;
    }
    
    int sz = howmany(maxfd, NFDBITS) * sizeof(fd_mask);
    if (writeset)
        memcpy(writeset, &rwriteset, sz);
    if (readset)
        memcpy(readset, &rreadset, sz);

    return ready;
}

int 
poll(struct pollfd *ufds, nfds_t nfds, int timeout) __THROW
{
    fd_set readset, writeset;
    struct timeval tv;

    FD_ZERO(&readset);
    FD_ZERO(&writeset);
    
    int max = -1;

    for (uint32_t i = 0; i < nfds; i++) {
	short e = ufds[i].events;
	int fd = ufds[i].fd;
	ufds[i].revents = 0;
	if ((e & POLLIN) || (e & POLLPRI))
	    FD_SET(fd, &readset);

	if (e & POLLOUT)
	    FD_SET(fd, &writeset);
	
	max = MAX(max, fd);
    }
    max = max + 1;

    struct timeval *tv2;
    if (timeout < 0)
	tv2 = 0;
    else {
	tv2 = &tv;
	memset(tv2, 0, sizeof(*tv2));
	tv2->tv_sec = timeout / 1000;
	tv2->tv_usec = (timeout % 1000) * 1000;
    }
    
    int r = select(max, &readset, &writeset, 0, tv2);
    if (r <= 0)
	return r;
    
    int r2 = 0;
    for (uint32_t i = 0; i < nfds; i++) {
	int fd = ufds[i].fd;
	char flag = 0;
	if (FD_ISSET(fd, &readset)) {
	    ufds[i].revents |= POLLIN;
	    flag = 1;
	}
	if (FD_ISSET(fd, &writeset)) {
	    ufds[i].revents |= POLLOUT;
	    flag = 1;
	}
	
	if (flag)
	    r2++;
    }
    
    return r2;
}

ssize_t
send(int fdnum, const void *dataptr, size_t size, int flags) __THROW
{
    return FD_CALL(fdnum, send, dataptr, size, flags);
}

ssize_t
sendto(int fdnum, const void *dataptr, size_t len, int flags, 
       const struct sockaddr *to, socklen_t tolen) __THROW
{
    return FD_CALL(fdnum, sendto, dataptr, len, flags, to, tolen);
}

ssize_t 
sendmsg(int s, const struct msghdr *msg, int flags) __THROW
{
    set_enosys();
    return -1;
}

ssize_t
recv(int fdnum, void *mem, size_t len, int flags) __THROW
{
    return FD_CALL(fdnum, recv, mem, len, flags);
}

ssize_t
recvfrom(int fdnum, void *mem, size_t len, int flags, 
         struct sockaddr *from, socklen_t *fromlen) __THROW
{
    set_enosys();
    return -1;
}

ssize_t 
recvmsg(int fdnum, struct msghdr *msg, int flags) __THROW
{
    set_enosys();
    return -1;
}

int 
fchdir(int fdnum) __THROW
{
    struct Fd *fd;
    int r;
    
    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0) {
	cprintf("fchdir(%d): %s\n", fdnum, e2s(r));
	__set_errno(EBADF);
	return -1;
    }
    
    if (fd->fd_dev_id != 'f') {
	cprintf("fchdir(%d): not a dir\n", fdnum);
	__set_errno(ENOTDIR);
	return -1;
    }
    start_env->fs_cwd = fd->fd_file.ino;
    return 0;
}

int
fstat64 (int __fd, struct stat64 *__buf) __THROW
{
    set_enosys();
    return -1;
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

    memset(buf, 0, sizeof(*buf));
    buf->st_dev = 1;	// some apps want a non-zero value
    buf->st_nlink = 1;

    return dev->dev_stat ? DEV_CALL(dev, stat, fd, buf) : 0;
}

int 
fstatfs(int fdnum, struct statfs *buf) __THROW
{
    
    struct Fd *fd;
    struct cobj_ref fd_obj;
    uint64_t fd_flags;
    int r;
    
    if ((r = fd_lookup(fdnum, &fd, &fd_obj, &fd_flags)) < 0) {
	__set_errno(EBADF);
	return -1;
    }
    
    if (fd->fd_dev_id != 'f') {
	cprintf("fstatfs(%d): not a file\n", fdnum);
	__set_errno(ENOTSUP);
	return -1;
    }
    
    struct fs_object_meta m;
    if (sys_obj_get_meta(fd->fd_file.ino.obj, &m) < 0) {
	__set_errno(EACCES);
	return -1;
    }
    
    memset(buf, 0, sizeof(*buf));
    buf->f_type = m.f_type;
    return 0;
}

int
statfs(const char *path, struct statfs *buf) __THROW
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
	return fd;

    int r = fstatfs(fd, buf);
    close(fd);

    return r;
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
	int newmode = (fd->fd_omode & ~mask) | (arg & mask);
	if (!(fd->fd_omode & O_ASYNC) && (newmode & O_ASYNC))
	    jos_sigio_enable(fdnum);
	if ((fd->fd_omode & O_ASYNC) && !(newmode & O_ASYNC))
	    jos_sigio_disable(fdnum);

	if (newmode != fd->fd_omode) {
	    if (fd->fd_immutable) {
		__set_errno(EPERM);
		return -1;
	    }

	    fd->fd_omode = newmode;
	}
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

    return fd->fd_offset;
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
    return 0;
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

    if ((r = fd_lookup(fdnum, &fd, 0, 0)) < 0) {
    	__set_errno(EBADF);
    	return -1;
    }

    va_start(ap, req);
    r = FD_CALL(fdnum, ioctl, req, ap);
    va_end(ap);
    return r;    
}

extern "C" ssize_t
__getdents (int fdnum, struct dirent *buf, size_t nbytes)
{
    return FD_CALL(fdnum, getdents, buf, nbytes);
}

extern "C" ssize_t
__getdents64(int fdnum, struct dirent *buf, uint64_t nbytes)
{
    return FD_CALL(fdnum, getdents, buf, nbytes);
}

int
ftruncate(int fdnum, off_t length) __THROW
{
    return FD_CALL(fdnum, trunc, length);
}

int ftruncate64 (int fdnum, off64_t length) __THROW
{
    return FD_CALL(fdnum, trunc, length);
}

int
fsync(int fdnum) __THROW
{
    return FD_CALL(fdnum, sync);
}

int 
fdatasync(int fdnum) __THROW
{
    return fsync(fdnum);
}

int
shutdown(int s, int how) __THROW
{
    return FD_CALL(s, shutdown, how);
}

weak_alias(__libc_fcntl, fcntl);
