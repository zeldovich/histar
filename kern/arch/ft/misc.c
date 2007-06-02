#include <machine/setjmp.h>
#include <machine/x86.h>
#include <kern/arch.h>
#include <kern/timer.h>

#include <time.h>
#include <sys/time.h>

uintptr_t
karch_get_sp(void)
{
    return read_esp();
}

uint64_t
karch_get_tsc(void)
{
    return read_tsc();
}

void
karch_jmpbuf_init(struct jos_jmp_buf *jb,
		  void *fn, void *stackbase)
{
    jb->jb_eip = (uintptr_t) fn;
    jb->jb_esp = (uintptr_t) ROUNDUP(stackbase, PGSIZE);
}

static void
lnx_schedtmr(void *arg, uint64_t nsec)
{
    cprintf("lnx_schedtmr: preemption in %"PRIu64" nsec\n", nsec);
}

static uint64_t
lnx_time_ticks(void *arg)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    uint64_t t = tv.tv_sec;
    t *= 1000000;
    return t + tv.tv_usec;
}

static void
lnx_time_delay(void *arg, uint64_t nsec)
{
    struct timespec ts;
    ts.tv_sec  = nsec / 1000000000;
    ts.tv_nsec = nsec % 1000000000;
    nanosleep(&ts, 0);
}

void
lnxtimer_init(void)
{
    static struct time_source timesrc;
    static struct preemption_timer schedtmr;

    the_timesrc = &timesrc;
    the_schedtmr = &schedtmr;

    timesrc.type = time_source_tsc;
    timesrc.freq_hz = 1000000;
    timesrc.ticks = &lnx_time_ticks;
    timesrc.delay_nsec = &lnx_time_delay;

    schedtmr.schedule_nsec = &lnx_schedtmr;
}
