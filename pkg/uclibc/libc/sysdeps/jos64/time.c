#include <inc/syscall.h>
#include <time.h>

time_t
time(time_t *tp)
{
    uint64_t msec = sys_clock_msec();
    time_t t = msec / 1000;

    if (tp)
	*tp = t;
    return t;
}
