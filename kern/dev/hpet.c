#include <machine/pmap.h>
#include <dev/hpet.h>
#include <dev/hpetreg.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>

struct hpet_state {
    struct hw_timer hwt;
    struct hpet_reg *reg;
    uint16_t min_tick;
};

static struct hpet_state the_hpet;

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
    uint64_t ticks = nsec * hpet->hwt.freq_hz / 1000000000;
    if (ticks < hpet->min_tick)
	ticks = hpet->min_tick;

    uint64_t now = hpet->reg->counter;
    uint64_t then = now + ticks;
    hpet->reg->timer[0].compare = then;

    now = hpet->reg->counter;
    int64_t diff = then - now;
    if (diff < 0) {
	hpet->reg->timer[0].compare = now + hpet->hwt.freq_hz;
	cprintf("hpet_schedule: lost a tick, recovering in a second\n");
    }
}

static void
hpet_delay(void *arg, uint64_t nsec)
{
    struct hpet_state *hpet = arg;
    uint64_t now = hpet->reg->counter;
    uint64_t diff = nsec * hpet->hwt.freq_hz / 1000000000;
    while ((hpet->reg->counter - now) < diff)
	;
}

void
hpet_attach(struct acpi_table_hdr *th)
{
    if (the_timer)
	return;

    struct hpet_state *hpet = &the_hpet;
    struct hpet_acpi *t = (struct hpet_acpi *) th;
    hpet->reg = pa2kva(t->base.addr);

    uint64_t cap = hpet->reg->cap;
    if (!(cap & HPET_CAP_LEGACY) || !(cap & HPET_CAP_64BIT)) {
	cprintf("HPET: incompetent chip: %"PRIx64"\n", cap);
	return;
    }

    uint64_t period = cap >> 32;
    hpet->hwt.freq_hz = UINT64(1000000000000000) / period;
    hpet->min_tick = t->min_tick;
    cprintf("HPET: %"PRIu64" Hz, min tick %d\n", hpet->hwt.freq_hz, hpet->min_tick);

    hpet->reg->config = HPET_CONFIG_ENABLE | HPET_CONFIG_LEGACY;

    /* Timer 0 is used in one-shot mode for preemption */
    hpet->reg->timer[0].config = HPET_TIMER_ENABLE;

    /* Timer 1 is used in periodic mode for periodic tasks */
    hpet->reg->timer[1].compare = hpet->hwt.freq_hz;
    hpet->reg->timer[1].config = HPET_TIMER_ENABLE | HPET_TIMER_ENABLE;

    /* Reset main counter to ensure the periodic counter starts */
    hpet->reg->counter = 0;

    static struct interrupt_handler irq0_ih = { .ih_func = &schedule };
    static struct interrupt_handler irq8_ih = { .ih_func = &timer_periodic_notify };
    irq_register(0, &irq0_ih);
    irq_register(8, &irq8_ih);


    cprintf("HPET: currently at %"PRIu64"\n", hpet->reg->counter);

    hpet->hwt.arg = hpet;
    hpet->hwt.ticks = &hpet_ticks;
    hpet->hwt.schedule = &hpet_schedule;
    hpet->hwt.delay = &hpet_delay;
    the_timer = &hpet->hwt;
}
