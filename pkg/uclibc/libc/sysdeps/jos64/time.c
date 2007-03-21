#include <inc/syscall.h>
#include <inc/time.h>
#include <inc/lib.h>
#include <inc/jthread.h>

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
    static struct time_of_day_seg *tods;
    if (!tods) {
	static jthread_mutex_t mu;
	jthread_mutex_lock(&mu);
	if (!tods) {
	    uint64_t bytes = 0;
	    int r = segment_map(start_env->time_seg, 0,
				SEGMAP_READ, (void **) &tods,
				&bytes, 0);
	    if (r < 0)
		cprintf("gettimeofday: cannot map time segment %ld.%ld: %s\n",
			start_env->time_seg.container,
			start_env->time_seg.object, e2s(r));

	    if (r >= 0 && bytes == 0) {
		segment_unmap(tods);
		tods = 0;
	    }
	}
	jthread_mutex_unlock(&mu);
    }

    uint64_t nsec = sys_clock_nsec() + (tods ? tods->unix_nsec_offset : 0);
    tv->tv_sec = nsec / NSEC_PER_SECOND;
    tv->tv_usec = (nsec % NSEC_PER_SECOND) / 1000;
    return 0;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    uint64_t start = sys_clock_nsec();
    uint64_t end = start + NSEC_PER_SECOND * req->tv_sec + req->tv_nsec;

    uint64_t now = start;
    do {
	sys_sync_wait(&now, now, end);
	now = sys_clock_nsec();
    } while (now < end);

    return 0;
}

clock_t
times(struct tms *buf)
{
    memset(buf, 0, sizeof(*buf));
    return sys_clock_nsec();
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
