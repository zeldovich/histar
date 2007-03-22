#include <machine/tsctimer.h>
#include <machine/x86.h>
#include <kern/timer.h>
#include <kern/lib.h>

static uint64_t
tsc_timer_ticks(void *arg)
{
    return read_tsc();
}

static void
tsc_timer_delay(void *arg, uint64_t nsec)
{
    uint64_t start = read_tsc();

    struct time_source *ts = arg;
    uint64_t delay = timer_convert(nsec, ts->freq_hz, 1000000000);
    while (read_tsc() - start < delay)
	;
}

void
tsc_timer_init(void)
{
    static struct time_source ts;
    if (!the_timesrc || the_timesrc->type != time_source_pmt)
	return;

    uint64_t tsc0 = read_tsc();
    uint64_t tick0 = the_timesrc->ticks(the_timesrc->arg);
    the_timesrc->delay_nsec(the_timesrc->arg, 10 * 1000 * 1000);
    uint64_t tsc1 = read_tsc();
    uint64_t tick1 = the_timesrc->ticks(the_timesrc->arg);

    uint64_t nsec = timer_convert(tick1 - tick0, 1000000000, the_timesrc->freq_hz);
    ts.freq_hz = timer_convert(tsc1 - tsc0, 1000000000, nsec);
    ts.arg = &ts;
    ts.ticks = &tsc_timer_ticks;
    ts.delay_nsec = &tsc_timer_delay;

    the_timesrc = &ts;
    cprintf("TSC timer: %"PRIu64" Hz\n", ts.freq_hz);
}
