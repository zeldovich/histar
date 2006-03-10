#include <inc/lib.h>
#include <unistd.h>

pid_t
getpid()
{
    return thread_id();
}

pid_t
getppid()
{
    return 0;
}
