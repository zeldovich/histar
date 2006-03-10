#include <inc/syscall.h>
#include <sys/time.h>
#include <errno.h>

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint64_t msec = sys_clock_msec();
    tv->tv_sec = msec / 1000;
    tv->tv_usec = (msec % 1000) * 1000;
}

int
alarm(int sec)
{
    __set_errno(ENOSYS);
    return -1;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    uint64_t start = sys_clock_msec();
    uint64_t end = start + req->tv_sec * 1000 + req->tv_nsec / 1000000;

    uint64_t now = start;
    do {
	sys_thread_sync_wait(&now, now, end);
	now = sys_clock_msec();
    } while (now < end);

    return 0;
}
