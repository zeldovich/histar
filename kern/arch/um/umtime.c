#include <kern/timer.h>
#include <machine/um.h>
#include <time.h>
#include <sys/time.h>

static void
um_timer_delay(void *arg, uint64_t nsec)
{
    struct timespec ts;
    ts.tv_sec = nsec / 1000000000;
    ts.tv_nsec = nsec % 1000000000;
    nanosleep(&ts, 0);
}

static uint64_t
um_timer_ticks(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t) tv.tv_sec) * 1000000 + tv.tv_usec;
}

void
um_time_init(void)
{
    static struct time_source ts = {
	.freq_hz = 1000000,
	.ticks = &um_timer_ticks,
	.delay_nsec = &um_timer_delay,
    };

    the_timesrc = &ts;
}
