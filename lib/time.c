#include <inc/syscall.h>
#include <inc/time.h>
#include <inc/lib.h>
#include <inc/jthread.h>
#include <inc/setjmp.h>

#include <errno.h>
#include <time.h>
#include <string.h>
#include <inttypes.h>

#include <sys/time.h>
#include <sys/times.h>
#include <sys/timex.h>

#include <bits/unimpl.h>

libc_hidden_proto(gettimeofday)
libc_hidden_proto(nanosleep)
libc_hidden_proto(adjtimex)
libc_hidden_proto(times)

uint64_t
jos_time_nsec_offset(void)
{
    static struct time_of_day_seg *tods;

 retry:
    if (!tods) {
	static jthread_mutex_t mu;
	jthread_mutex_lock(&mu);
	if (!tods) {
	    uint64_t bytes = 0;
	    int r = segment_map(start_env->time_seg, 0,
				SEGMAP_READ | SEGMAP_VECTOR_PF,
				(void **) &tods, &bytes, 0);
	    if (r < 0)
		cprintf("jos_time_nsec: cannot map time segment "
			"%"PRIu64".%"PRIu64": %s\n",
			start_env->time_seg.container,
			start_env->time_seg.object, e2s(r));

	    if (r >= 0 && bytes == 0) {
		segment_unmap(tods);
		tods = 0;
	    }
	}
	jthread_mutex_unlock(&mu);
    }

    if (!tods)
	return 0;

    uint64_t offset;
    volatile struct jos_jmp_buf *old_jb = tls_data->tls_pgfault;
    struct jos_jmp_buf jb;
    if (jos_setjmp(&jb) != 0) {
	tls_data->tls_pgfault = old_jb;
	int64_t init_ct = container_find(start_env->root_container,
					 kobj_container, "init");
	if (init_ct < 0)
	    return 0;

	int64_t tods_sg = container_find(init_ct, kobj_segment, "time-of-day");
	if (tods_sg < 0)
	    return 0;

	start_env->time_seg = COBJ(init_ct, tods_sg);
	segment_unmap(tods);
	tods = 0;
	goto retry;
    }
    tls_data->tls_pgfault = &jb;

    offset = tods->unix_nsec_offset;
    tls_data->tls_pgfault = old_jb;
    return offset;
}

uint64_t
jos_time_nsec(void)
{
    return sys_clock_nsec() + jos_time_nsec_offset();
}

int
gettimeofday(struct timeval *tv, struct timezone *tz)
{
    uint64_t nsec = jos_time_nsec();
    tv->tv_sec = nsec / NSEC_PER_SECOND;
    tv->tv_usec = (nsec % NSEC_PER_SECOND) / 1000;
    return 0;
}

int
nanosleep(const struct timespec *req, struct timespec *rem)
{
    uint64_t start = sys_clock_nsec();
    uint64_t end = start + NSEC_PER_SECOND * req->tv_sec + req->tv_nsec;

    sys_sync_wait(&start, start, end);
    uint64_t stop = sys_clock_nsec();

    if (stop > end)
	stop = end;

    if (rem) {
	rem->tv_sec = (end - stop) / NSEC_PER_SECOND;
	rem->tv_nsec = (end - stop) % NSEC_PER_SECOND;
    }

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

int
adjtimex(struct timex *buf)
{
    set_enosys();
    return -1;
}

libc_hidden_def(gettimeofday)
libc_hidden_def(nanosleep)
libc_hidden_def(adjtimex)
libc_hidden_def(times)

