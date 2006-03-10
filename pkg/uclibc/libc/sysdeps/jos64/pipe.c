#include <unistd.h>
#include <errno.h>

int
pipe(int fds[2])
{
    __set_errno(ENOSYS);
    return -1;
}
