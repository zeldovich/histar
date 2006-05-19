#include <inc/lib.h>
#include <inc/segment.h>
#include <inc/syscall.h>
#include <inc/memlayout.h>

#include <errno.h>
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
    r = segment_map(seg, seg_flags, &va, 0);
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
    struct u_segment_mapping usm;
    int r = segment_lookup(start, &usm);
    if (r < 0) {
	__set_errno(EINVAL);
	return -1;
    }

    struct cobj_ref seg = usm.segment;
    uint64_t npage = usm.num_pages;
    uint64_t flags = usm.flags;

    if (!(flags & SEGMAP_ANON_MMAP)) {
	set_enosys();
	return -1;
    }

    if (ROUNDUP(length, PGSIZE) != npage * PGSIZE) {
	cprintf("munmap: unmapping different length: length=%ld, npage=%ld\n",
		length, npage);
	__set_errno(EINVAL);
	return -1;
    }

    r = segment_unmap(start);
    if (r < 0) {
	cprintf("munmap: unable to unmap: %s\n", e2s(r));
	__set_errno(EPERM);
	return -1;
    }

    sys_obj_unref(seg);
    return 0;
}

void *
mremap(void *old_address, size_t old_size, size_t new_size, int may_move)
{
    set_enosys();
    return MAP_FAILED;
}
