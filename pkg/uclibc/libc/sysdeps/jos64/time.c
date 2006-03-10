#include <inc/syscall.h>
#include <sys/time.h>

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint64_t msec = sys_clock_msec();
    tv->tv_sec = msec / 1000;
    tv->tv_usec = (msec % 1000) * 1000;
}
