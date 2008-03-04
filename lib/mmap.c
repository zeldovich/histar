#include <inc/lib.h>
#include <inc/segment.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>
#include <inc/fd.h>

#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <inc/debug.h>

#include <bits/unimpl.h>

libc_hidden_proto(mmap)
libc_hidden_proto(munmap)
libc_hidden_proto(mremap)
libc_hidden_proto(msync)
libc_hidden_proto(mprotect)

void *
mmap64(void *start, size_t length, int prot, int flags, int fd, __off64_t offset)
{
    jos_trace("%p, %zu, %x, %x, %d, %zu", start, length, prot, flags, fd,
              offset);

    return mmap(start, length, prot, flags, fd, offset);
}

void *
mmap(void *start, size_t length, int prot, int flags, int fdnum, off_t offset)
{
    jos_trace("%p, %zu, %x, %x, %d, %zu", start, length, prot, flags, fdnum,
              offset);

    if (!(flags & MAP_ANONYMOUS)) {
	struct Fd *fd;
	int r = fd_lookup(fdnum, &fd, 0, 0);
	if (r < 0) {
	    __set_errno(EBADF);
	    return MAP_FAILED;
	}

	if (fd->fd_dev_id == devzero.dev_id)
	    goto anon;

	if (fd->fd_dev_id != devfile.dev_id) {
	    cprintf("mmap: cannot mmap type %d\n", fd->fd_dev_id);
	    __set_errno(EINVAL);
	    return MAP_FAILED;
	}

	struct cobj_ref seg = fd->fd_file.ino.obj;
	if (flags & MAP_PRIVATE) {
	    int64_t copy_id = sys_segment_copy(seg, start_env->proc_container,
					       0, "mmap MAP_PRIVATE copy");
	    if (copy_id < 0) {
		__set_errno(EINVAL);
		return MAP_FAILED;
	    }

	    seg = COBJ(start_env->proc_container, copy_id);
	}

	int map_flags = SEGMAP_READ;
	if (prot & PROT_EXEC)
	    map_flags |= SEGMAP_EXEC;
	if (prot & PROT_WRITE)
	    map_flags |= SEGMAP_WRITE;

	void *va = (flags & MAP_FIXED) ? start : 0;
	uint64_t map_bytes = length;
	r = segment_map(seg, offset, map_flags, &va, &map_bytes,
			(flags & MAP_FIXED) ? SEG_MAPOPT_REPLACE : 0);
	if (r < 0) {
	    __set_errno(EINVAL);
	    return MAP_FAILED;
	}

	return va;
    }

    uint32_t seg_flags;
 anon:
    seg_flags = SEGMAP_ANON_MMAP | SEGMAP_READ;
    if ((prot & PROT_EXEC))
	seg_flags |= SEGMAP_EXEC;
    if ((prot & PROT_WRITE))
	seg_flags |= SEGMAP_WRITE;

    struct cobj_ref seg;
    int r = segment_alloc(start_env->proc_container, length,
			  &seg, 0, 0, "anon mmap");
    if (r < 0) {
	__set_errno(ENOMEM);
	return MAP_FAILED;
    }

    void *va = (flags & MAP_FIXED) ? start : 0;
    r = segment_map(seg, 0, seg_flags, &va, 0, 0);
    if (r < 0) {
	sys_obj_unref(seg);
	__set_errno(ENOMEM);
	return MAP_FAILED;
    }

    return va;
}

int
munmap(void *start, size_t length)
{
    jos_trace("%p, %zu", start, length);

    if (PGOFF(start)) {
	cprintf("munmap: unaligned unmap, va %p, length %zu\n", start, length);
	__set_errno(EINVAL);
	return -1;
    }

    length = ROUNDUP(length, PGSIZE);

    struct u_segment_mapping omap;
    int r = segment_lookup(start, &omap);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    if (!(omap.flags & SEGMAP_ANON_MMAP)) {
	if (omap.va == start &&
	    omap.num_pages == ROUNDUP(length, PGSIZE) / PGSIZE)
	{
	    r = segment_unmap(start);
	    if (r < 0) {
		cprintf("munmap: segment_unmap: %s\n", e2s(r));
		__set_errno(EINVAL);
		return -1;
	    }

	    return 0;
	}

	set_enosys();
	return -1;
    }

    /*
     * original mapping (omap) can extend past [start .. start+length):
     *
     *    omap.va     start   start+length   omap.end
     *       |----------|-----------|-----------|
     *	         left                   right
     */
    uint64_t left_pages = (((uintptr_t)start) - ((uintptr_t)omap.va)) / PGSIZE;
    uint64_t right_pages = omap.num_pages - length / PGSIZE - left_pages;

    void *right_va = 0;
    if (right_pages) {
	uint64_t nbytes = right_pages * PGSIZE;
	right_va = start + length;
	r = segment_map(omap.segment,
			(omap.start_page + left_pages) * PGSIZE + length,
			omap.flags, &right_va, &nbytes,
			SEG_MAPOPT_OVERLAP);
	if (r < 0) {
	    cprintf("munmap: unable to map right chunk: %s\n", e2s(r));
	    __set_errno(EPERM);
	    return -1;
	}
    }

    if (left_pages) {
	uint64_t nbytes = left_pages * PGSIZE;
	void *va = omap.va;
	r = segment_map(omap.segment, omap.start_page * PGSIZE,
			omap.flags, &va, &nbytes,
			SEG_MAPOPT_REPLACE);
    } else {
	r = segment_unmap(omap.va);
    }

    if (r < 0) {
	cprintf("munmap: unable to unmap: %s\n", e2s(r));
	if (right_va)
	    segment_unmap(right_va);

	__set_errno(EPERM);
	return -1;
    }

    r = segment_lookup_obj(omap.segment.object, 0);
    if (r == 0)
	sys_obj_unref(omap.segment);

    return 0;
}

void *
mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...)
{
    jos_trace("%p, %zu, %zu, %x ...", old_address, old_size, new_size, flags);

    __set_errno(ENOMEM);
    return MAP_FAILED;
}

int
msync(void *start, size_t length, int flags)
{
    jos_trace("%p, %zu, %x", start, length, flags);

    set_enosys();
    return -1;
}

int
mprotect(void *addr, size_t len, int prot)
{
    jos_trace("%p, %zu, %x", addr, len, prot);

    struct u_segment_mapping usm;
    int r = segment_lookup(addr, &usm);
    if (r <= 0) {
	__set_errno(EINVAL);
	return -1;
    }

    if (prot & PROT_EXEC)
	usm.flags |= SEGMAP_EXEC;
    if (prot & PROT_READ)
	usm.flags |= SEGMAP_READ;
    if (prot & PROT_WRITE)
	usm.flags |= SEGMAP_WRITE;

    uint64_t nbytes = usm.num_pages * PGSIZE;
    r = segment_map(usm.segment, usm.start_page * PGSIZE, usm.flags,
		    &usm.va, &nbytes, SEG_MAPOPT_REPLACE);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    return 0;
}

int
mlock(const void *addr, size_t len)
{
    jos_trace("%p, %zu", addr, len);

    set_enosys();
    return -1;
}

int
munlock(const void *addr, size_t len)
{
    jos_trace("%p, %zu", addr, len);

    set_enosys();
    return -1;
}

int
mlockall(int flags)
{
    jos_trace("%x", flags);

    set_enosys();
    return -1;
}

int
munlockall(void)
{
    jos_trace("");

    set_enosys();
    return -1;
}

libc_hidden_def(mmap)
libc_hidden_def(munmap)
libc_hidden_def(mremap)
libc_hidden_def(msync)
libc_hidden_def(mprotect)

