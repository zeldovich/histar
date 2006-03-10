#include <unistd.h>
#include <errno.h>

#include <bits/unimpl.h>

int
pipe(int fds[2])
{
    set_enosys();
    return -1;
}
