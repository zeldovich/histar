#include <errno.h>
#include <sys/resource.h>

#include <bits/unimpl.h>

int
getrlimit(__rlimit_resource_t resource, struct rlimit *rlim)
{
    set_enosys();
    return -1;
}

int
getrusage(int who, struct rusage *usage)
{
    __set_errno(ENOSYS);
    return -1;
}

int
setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim)
{
    set_enosys();
    return -1;
}
