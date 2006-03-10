#include <errno.h>
#include <sys/resource.h>

int
getrlimit(int resource, struct rlimit *rlim)
{
    __set_errno(ENOSYS);
    return -1;
}

int
getrusage(int who, struct rusage *usage)
{
    __set_errno(ENOSYS);
    return -1;
}

int
setrlimit(int resource, const struct rlimit *rlim)
{
    __set_errno(ENOSYS);
    return -1;
}
