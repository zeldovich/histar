#include <sys/types.h>
#include <errno.h>

ssize_t
__getdents (int fd, char *buf, size_t nbytes)
{
    __set_errno(ENOSYS);
    return -1;
}
