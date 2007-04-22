#include <inc/lib.h>
#include <inc/segment.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>

#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>

#include <bits/unimpl.h>

void *
mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset)
{
    if (!(flags & MAP_ANONYMOUS)) {
	set_enosys();
	return MAP_FAILED;
    }

    if (start && (flags & MAP_FIXED)) {
	__set_errno(EINVAL);
	return MAP_FAILED;
    }

    uint32_t seg_flags = SEGMAP_ANON_MMAP | SEGMAP_READ;
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

    void *va = 0;
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
    if ((length % PGSIZE) != 0) {
	cprintf("munmap: unaligned unmap, va %p, length %zu\n", start, length);
	__set_errno(EINVAL);
	return -1;
    }

    struct u_segment_mapping omap;
    int r = segment_lookup(start, &omap);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    if (!(omap.flags & SEGMAP_ANON_MMAP)) {
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
mremap(void *old_address, size_t old_size, size_t new_size, int may_move)
{
    __set_errno(ENOMEM);
    return MAP_FAILED;
}

int msync(void *start, size_t length, int flags)
{
    set_enosys();
    return -1;
}
