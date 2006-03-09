#include <errno.h>
#include <sys/mman.h>

void *
mmap(void *start, size_t length, int prot , int flags, int fd, off_t offset)
{
    if ((flags & MAP_ANONYMOUS)) {
	__set_errno(EINVAL);
	return MAP_FAILED;
    }

    __set_errno(ENOSYS);
    return MAP_FAILED;
}

int
munmap(void *start, size_t length)
{
    __set_errno(ENOSYS);
    return -1;
}

void *
mremap(void *old_address, size_t old_size, size_t new_size, int may_move)
{
    __set_errno(ENOSYS);
    return MAP_FAILED;
}
