#include <dev/hpet.h>
#include <dev/hpetreg.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/arch.h>
#include <kern/lib.h>

struct hpet_state {
    struct time_source timesrc;
    struct preemption_timer preempt;

    struct hpet_reg *reg;
    uint16_t min_tick;
};

static uint64_t
hpet_ticks(void *arg)
{
    struct hpet_state *hpet = arg;
    return hpet->reg->counter;
}

static void
hpet_schedule(void *arg, uint64_t nsec)
{
    struct hpet_state *hpet = arg;
    uint64_t ticks = timer_convert(nsec, hpet->timesrc.freq_hz, 1000000000);
    if (ticks < hpet->min_tick)
	ticks = hpet->min_tick;

    uint64_t now = hpet->reg->counter;
    uint64_t then = now + ticks;
    hpet->reg->timer[0].compare = then;

    now = hpet->reg->counter;
    int64_t diff = then - now;
    if (diff < 0) {
	hpet->reg->timer[0].compare = now + hpet->timesrc.freq_hz;
	cprintf("hpet_schedule: lost a tick, recovering in a second\n");
    }
}

static void
hpet_delay(void *arg, uint64_t nsec)
{
    struct hpet_state *hpet = arg;
    uint64_t now = hpet->reg->counter;
    uint64_t diff = timer_convert(nsec, hpet->timesrc.freq_hz, 1000000000);
    while ((hpet->reg->counter - now) < diff)
	;
}

static void
hpet_intr(void *arg)
{
    schedule();
}

void
hpet_attach(struct acpi_table_hdr *th)
{
    static struct hpet_state the_hpet;
    struct hpet_state *hpet = &the_hpet;

    if (the_timesrc && the_schedtmr)
	return;

    struct hpet_acpi *t = (struct hpet_acpi *) th;
    hpet->reg = pa2kva(t->base.addr);

    uint64_t cap = hpet->reg->cap;
    if (!(cap & HPET_CAP_LEGACY) || !(cap & HPET_CAP_64BIT)) {
	cprintf("HPET: incompetent chip: %"PRIx64"\n", cap);
	return;
    }

    uint64_t period = cap >> 32;
    hpet->timesrc.freq_hz = UINT64(1000000000000000) / period;
    hpet->min_tick = t->min_tick;
    cprintf("HPET: %"PRIu64" Hz, min tick %d\n", hpet->timesrc.freq_hz, hpet->min_tick);

    hpet->reg->config = HPET_CONFIG_ENABLE | HPET_CONFIG_LEGACY;

    /* Timer 0 is used in one-shot mode for preemption */
    hpet->reg->timer[0].config = HPET_TIMER_ENABLE;

    /* Reset main counter to ensure the periodic counter starts */
    hpet->reg->counter = 0;

    static struct interrupt_handler irq0_ih = { .ih_func = &hpet_intr };
    irq_register(0, &irq0_ih);

    hpet->timesrc.type = time_source_hpet;
    hpet->timesrc.arg = hpet;
    hpet->timesrc.ticks = &hpet_ticks;
    hpet->timesrc.delay_nsec = &hpet_delay;
    the_timesrc = &hpet->timesrc;

    hpet->preempt.arg = hpet;
    hpet->preempt.schedule_nsec = &hpet_schedule;
    the_schedtmr = &hpet->preempt;
}
