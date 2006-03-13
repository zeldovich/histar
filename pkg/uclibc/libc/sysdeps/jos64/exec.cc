extern "C" {
#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>

#include <bits/unimpl.h>
}

int
execve(const char * filename, char *const * argv, char *const * envp) __THROW
{
    set_enosys();
    return -1;
}
