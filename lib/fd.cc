extern "C" {
#include <inc/stdio.h>
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <fcntl.h>
}

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

// Maximum number of file descriptors a program may hold open concurrently
#define MAXFD		32
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

static int debug = 0;


/********************************
 * FILE DESCRIPTOR MANIPULATORS *
 *                              *
 ********************************/

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
	int r = fd_lookup(i, &fd, 0);
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
int
fd_alloc(uint64_t container, struct Fd **fd_store, const char *name)
{
	fd_handles_init();

	int i;
	struct Fd *fd;

	for (i = 0; i < MAXFD; i++) {
		fd = INDEX2FD(i);
		int r = segment_lookup(fd, 0, 0);
		if (r < 0)
			return r;
		if (r == 0)
			break;
	}

	*fd_store = 0;
	if (i == MAXFD)
		return -E_MAX_OPEN;

	int64_t fd_grant = sys_handle_create();
	if (fd_grant < 0)
		return fd_grant;
	scope_guard<void, uint64_t> grant_drop(thread_drop_star, fd_grant);

	int64_t fd_taint = sys_handle_create();
	if (fd_taint < 0)
		return fd_taint;
	scope_guard<void, uint64_t> taint_drop(thread_drop_star, fd_taint);

	label l;
	try {
		thread_cur_label(&l);
		l.transform(label::star_to, l.get_default());
		l.set(fd_grant, 0);
		l.set(fd_taint, 3);
	} catch (error &e) {
		cprintf("fd_alloc: %s\n", e.what());
		return e.err();
	}

	struct cobj_ref seg;
	int r = segment_alloc(container, PGSIZE, &seg,
			      (void**)&fd, l.to_ulabel(), name);
	if (r < 0)
		return r;

	atomic_set(&fd->fd_ref, 1);
	fd->fd_dev_id = 0;
	fd->fd_grant = fd_grant;
	fd->fd_taint = fd_taint;
	fd_handles[fd2num(fd)].fd_grant = fd_grant;
	fd_handles[fd2num(fd)].fd_taint = fd_taint;

	grant_drop.dismiss();
	taint_drop.dismiss();

	*fd_store = fd;
	return 0;
}

// Check that fdnum is in range and mapped.
// If it is, set *fd_store to the fd page virtual address.
//
// Returns 0 on success (the page is in range and mapped), < 0 on error.
// Errors are:
//	-E_INVAL: fdnum was either not in range or not mapped.
int
fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp)
{
	if (fdnum < 0 || fdnum >= MAXFD) {
		if (debug)
			cprintf("[%lx] bad fd %d\n", thread_id(), fdnum);
		return -E_INVAL;
	}
	struct Fd *fd = INDEX2FD(fdnum);

	int r = segment_lookup(fd, objp, 0);
	if (r < 0)
		return r;
	if (r == 0) {
		if (debug)
			cprintf("[%lx] closed fd %d\n", thread_id(), fdnum);
		return -E_INVAL;
	}

	*fd_store = fd;
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

	if (fd->fd_immutable || !atomic_dec_and_test(&fd->fd_ref))
		goto out;

	struct cobj_ref fd_seg;
	r = segment_lookup(fd, &fd_seg, 0);
	if (r < 0)
		return r;
	if (r == 0)
		return -E_NOT_FOUND;

	struct Dev *dev;
	r = dev_lookup(fd->fd_dev_id, &dev);
	if (r < 0)
		return r;

	r = (*dev->dev_close)(fd);

	sys_obj_unref(fd_seg);

out:
	if (handle_refs == 2) try {
		fd_give_up_privilege(fd2num(fd));
	} catch (std::exception &e) {
		cprintf("fd_close: cannot drop handle: %s\n", e.what());
	}
	fd_handles[fd2num(fd)].fd_taint = 0;
	fd_handles[fd2num(fd)].fd_grant = 0;
	segment_unmap(fd);
	return r;
}

int
fd_move(int fdnum, uint64_t container)
{
    struct Fd *fd;
    struct cobj_ref oldseg;
    int r = fd_lookup(fdnum, &fd, &oldseg);
    if (r < 0)
	return r;

    if (oldseg.container == container)
	return 0;

    r = sys_segment_addref(oldseg, container);
    if (r < 0)
	return r;

    // XXX the failure semantics are not very good: if we fail
    // to remap some of the FDs mapping this segment, we leave
    // the segment referenced in both of the containers, and
    // with FDs referencing both containers.  so we assert().

    struct cobj_ref newseg = COBJ(container, oldseg.object);
    for (int i = 0; i < MAXFD; i++) {
	r = fd_lookup(i, &fd, &oldseg);
	if (r < 0)
	    continue;

	if (oldseg.object == newseg.object &&
	    oldseg.container != newseg.container)
	{
	    int perm = fd->fd_immutable ? SEGMAP_READ
					: SEGMAP_READ | SEGMAP_WRITE;
	    assert(0 == segment_unmap(fd));
	    assert(0 == segment_map(newseg, perm, (void **) &fd, 0));
	}
    }

    sys_obj_unref(oldseg);
    return 0;
}

void
fd_give_up_privilege(int fdnum)
{
    thread_drop_star(fd_handles[fdnum].fd_taint);
    thread_drop_star(fd_handles[fdnum].fd_grant);
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
close(int fdnum)
{
	struct Fd *fd;
	int r;

	if ((r = fd_lookup(fdnum, &fd, 0)) < 0)
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
dup2(int oldfdnum, int newfdnum)
{
	struct Fd *oldfd;
	struct cobj_ref fd_seg;
	int r = fd_lookup(oldfdnum, &oldfd, &fd_seg);
	if (r < 0)
		return r;

	close(newfdnum);
	struct Fd *newfd = INDEX2FD(newfdnum);

	int immutable = oldfd->fd_immutable;
	r = segment_map(fd_seg,
			SEGMAP_READ | (immutable ? 0 : SEGMAP_WRITE),
			(void**) &newfd, 0);
	if (r < 0)
		return r;

	if (!immutable)
		atomic_inc(&oldfd->fd_ref);
	return newfdnum;
}

int
dup2_as(int oldfdnum, int newfdnum, struct cobj_ref target_as)
{
	struct Fd *oldfd;
	struct cobj_ref fd_seg;
	int r = fd_lookup(oldfdnum, &oldfd, &fd_seg);
	if (r < 0)
		return r;

	// XXX only works for initial setup, as this doesn't close
	// newfdnum in target address space if one aleady exists.
	struct Fd *newfd = INDEX2FD(newfdnum);

	int immutable = oldfd->fd_immutable;
	r = segment_map_as(target_as, fd_seg,
			   SEGMAP_READ | (immutable ? 0 : SEGMAP_WRITE),
			   (void**) &newfd, 0);
	if (r < 0)
		return r;

	if (!immutable)
		atomic_inc(&oldfd->fd_ref);
	return newfdnum;
}

ssize_t
read(int fdnum, void *buf, size_t n)
{
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd, 0)) < 0
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
	int m;

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
write(int fdnum, const void *buf, size_t n)
{
	int r;
	struct Dev *dev;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd, 0)) < 0
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
seek(int fdnum, off_t offset)
{
	int r;
	struct Fd *fd;

	if ((r = fd_lookup(fdnum, &fd, 0)) < 0)
		return r;
	if (fd->fd_immutable)
		return -E_BAD_OP;
	fd->fd_offset = offset;
	return 0;
}

int
bind(int fdnum, struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_bind(fd, addr, addrlen);
}

int
connect(int fdnum, struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_connect(fd, addr, addrlen);
}

int
listen(int fdnum, int backlog)
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_listen(fd, backlog);
}

int
accept(int fdnum, struct sockaddr *addr, socklen_t *addrlen)
{
    int r;
    struct Fd *fd;
    struct Dev *dev;

    if ((r = fd_lookup(fdnum, &fd, 0)) < 0
	|| (r = dev_lookup(fd->fd_dev_id, &dev)) < 0)
	    return r;

    return dev->dev_accept(fd, addr, addrlen);
}
