#include <sys/types.h>
#include <errno.h>
#include <bits/unimpl.h>

ssize_t
__getdents (int fd, char *buf, size_t nbytes)
{
    set_enosys();
    return -1;
}
