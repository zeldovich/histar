#include <inc/lib.h>
#include <unistd.h>
#include <errno.h>

#include <bits/unimpl.h>

int
execve(const char * filename, char *const * argv, char *const * envp)
{
    set_enosys();
    return -1;
}
