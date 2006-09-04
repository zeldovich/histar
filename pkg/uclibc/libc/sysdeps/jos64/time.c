#include <inc/syscall.h>

#include <errno.h>
#include <time.h>
#include <string.h>

#include <sys/time.h>
#include <sys/times.h>
#include <time.h>

#include <bits/unimpl.h>

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint64_t msec = sys_clock_msec();
    tv->tv_sec = msec / 1000;
    tv->tv_usec = (msec % 1000) * 1000;
    return 0;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    uint64_t start = sys_clock_msec();
    uint64_t end = start + req->tv_sec * 1000 + req->tv_nsec / 1000000;

    uint64_t now = start;
    do {
	sys_sync_wait(&now, now, end);
	now = sys_clock_msec();
    } while (now < end);

    return 0;
}

clock_t
times(struct tms *buf)
{
    memset(buf, 0, sizeof(*buf));
    return sys_clock_msec();
}

int
clock_gettime(clockid_t clock_id, struct timespec *tp)
{
    struct timeval tv;
    int retval = -1;

    switch (clock_id) {
    case CLOCK_REALTIME:
	retval = gettimeofday(&tv, NULL);
	if (retval == 0) {
	    tp->tv_sec = tv.tv_sec;
	    tp->tv_nsec = tv.tv_usec * 1000;
	}
	break;
	
    default:
	errno = EINVAL;
	break;
    }
    
    return retval;
}

int 
clock_getres (clockid_t clock_id, struct timespec *res)
{
    int retval = -1;

    switch (clock_id) {
    case CLOCK_REALTIME:
	res->tv_sec = 0;
	res->tv_nsec = 1000000;
	retval = 0;
	break;
	
    default:
	errno = EINVAL;
	break;
    }
    
    return retval;
}
