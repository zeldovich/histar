#include <machine/x86.h>
#include <dev/isareg.h>
#include <dev/timerreg.h>
#include <dev/kclock.h>
#include <dev/picirq.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/sched.h>
#include <kern/timer.h>

static int pit_hz = 100;
static int pit_tval;
static struct hw_timer pit_timer;
static uint64_t pit_ticks;

unsigned
mc146818_read(unsigned reg)
{
    outb(IO_RTC, reg);
    return inb(IO_RTC + 1);
}

void
mc146818_write(unsigned reg, unsigned datum)
{
    outb(IO_RTC, reg);
    outb(IO_RTC + 1, datum);
}

static void
pit_intr(void)
{
    pit_ticks++;
    timer_periodic_notify();
    schedule();
}

static uint64_t
pit_get_ticks(void *arg)
{
    return pit_ticks;
}

static void
pit_schedule(void *arg, uint64_t nsec)
{
}

static int
pit_gettick(void)
{
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);
    int lo = inb(TIMER_CNTR0);
    int hi = inb(TIMER_CNTR0);
    return (hi << 8) | lo;
}

static void
pit_delay(void *arg, uint64_t nsec)
{
    uint64_t usec = nsec / 1000;
    int tick_start = pit_gettick();

    // This obtuse code comes from NetBSD sys/arch/amd64/isa/clock.c
    int t_sec = usec / 1000000;
    int t_usec = usec % 1000000;
    int ticks = t_sec * TIMER_FREQ +
		t_usec * (TIMER_FREQ / 1000000) +
		t_usec * ((TIMER_FREQ % 1000000) / 1000) / 1000 +
		t_usec * (TIMER_FREQ % 1000) / 1000000;

    while (ticks > 0) {
	int tick_now = pit_gettick();
	if (tick_now > tick_start)
	    ticks -= pit_tval - (tick_now - tick_start);
	else
	    ticks -= tick_start - tick_now;
	tick_start = tick_now;
    }
}

void
pit_init(void)
{
    if (the_timer)
	return;

    /* initialize 8253 clock to interrupt pit_hz times/sec */
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);

    pit_tval = TIMER_DIV(pit_hz);
    outb(IO_TIMER1, pit_tval % 256);
    outb(IO_TIMER1, pit_tval / 256);
    irq_setmask_8259A(irq_mask_8259A & ~(1 << 0));

    static struct interrupt_handler pit_ih = { .ih_func = &pit_intr };
    irq_register(0, &pit_ih);

    pit_timer.freq_hz = pit_hz;
    pit_timer.ticks = &pit_get_ticks;
    pit_timer.schedule = &pit_schedule;
    pit_timer.delay = &pit_delay;
    the_timer = &pit_timer;
}
