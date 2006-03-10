#include <errno.h>
#include <sys/mman.h>

#include <bits/unimpl.h>

void *
mmap(void *start, size_t length, int prot , int flags, int fd, off_t offset)
{
    if ((flags & MAP_ANONYMOUS)) {
	__set_errno(EINVAL);
	return MAP_FAILED;
    }

    set_enosys();
    return MAP_FAILED;
}

int
munmap(void *start, size_t length)
{
    set_enosys();
    return -1;
}

void *
mremap(void *old_address, size_t old_size, size_t new_size, int may_move)
{
    set_enosys();
    return MAP_FAILED;
}
