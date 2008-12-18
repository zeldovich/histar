#include <machine/nacl.h>
#include <machine/x86.h>
#include <kern/timer.h>
#include <kern/lib.h>

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <malloc.h>

static uint64_t
nacl_timer_ticks(void *arg)
{
    return read_tsc();
}

static void
nacl_timer_delay(void *arg, uint64_t nsec)
{
    struct timespec ts;
    ts.tv_sec = nsec / 1000000000;
    ts.tv_nsec = nsec % 1000000000;
    nanosleep(&ts, 0);
}

static void
nacl_schedule_nsec(void *arg, uint64_t nsec)
{
}

void
nacl_timer_init(void)
{
    static struct preemption_timer pt;
    static struct time_source ts;
    FILE *f;
    char *line = 0;
    size_t len = 0;
    float freq = 0;
    
    f = fopen("/proc/cpuinfo", "r");
    if (!f)
	panic("failed to get cpu frequecy");
    
    while (getline(&line, &len, f) != EOF) {
	if (sscanf(line, "cpu MHz\t: %f", &freq) == 1) {
	    freq = freq * UINT64(1000000);
	    break;
	}
	
    }
    assert(freq);
    fclose(f);
    free(line);
    
    ts.freq_hz = freq;
    ts.arg = &ts;
    ts.ticks = &nacl_timer_ticks;
    ts.delay_nsec = &nacl_timer_delay;

    pt.schedule_nsec = &nacl_schedule_nsec,

    the_timesrc = &ts;
    the_schedtmr = &pt;

    cprintf("TSC timer: %"PRIu64" Hz\n", ts.freq_hz);
}
