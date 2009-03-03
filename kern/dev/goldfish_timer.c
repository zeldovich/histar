#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <dev/goldfish_timer.h>
#include <dev/goldfish_timerreg.h>

struct goldfish_timer_state {
	struct time_source timesrc;
	struct preemption_timer preempt;

	struct goldfish_timer_reg *reg;
};

static uint64_t
goldfish_timer_getticks(void *arg)
{
	struct goldfish_timer_state *gft = arg;
	uint64_t lo, hi;

	lo = gft->reg->timer_low;
	hi = gft->reg->timer_high;

	return (lo | (hi << 32));
}

static void
goldfish_alarm_setticks(void *arg, uint64_t ticks)
{
	struct goldfish_timer_state *gft = arg;

	gft->reg->alarm_high = ticks >> 32;
	gft->reg->alarm_low  = ticks & 0xffffffff;
}

static void
goldfish_timer_schedule(void *arg, uint64_t nsec)
{
	struct goldfish_timer_state *gft = arg;
	uint64_t ticks;

	ticks = timer_convert(nsec, gft->timesrc.freq_hz, 1000000000);

	uint64_t now = goldfish_timer_getticks(gft);
	uint64_t then = now + ticks;
	goldfish_alarm_setticks(gft, then);

	now = goldfish_timer_getticks(gft);
	int64_t diff = then - now;
	if (diff < 0) {
		goldfish_alarm_setticks(gft, now + gft->timesrc.freq_hz);
		cprintf("%s: lost a tick, recovering in a second\n", __func__);
	}
}

static void
goldfish_timer_delay(void *arg, uint64_t nsec)
{
	struct goldfish_timer_state *gft = arg;
	uint64_t now = goldfish_timer_getticks(gft); 
	uint64_t diff = timer_convert(nsec, gft->timesrc.freq_hz, 1000000000);

	while ((goldfish_timer_getticks(gft) - now) < diff)
		;
}

static void
goldfish_timer_intr(void *arg)
{
	struct goldfish_timer_state *gft = arg;

	assert(arg != NULL);
	gft->reg->alarm_clear = 0;
	schedule();
}

void
goldfish_timer_init()
{
	static struct goldfish_timer_state the_goldfish_timer;
	static struct interrupt_handler goldfish_timer_ih = {
		.ih_func = &goldfish_timer_intr,
		.ih_arg = &the_goldfish_timer
	};
	struct goldfish_timer_state *gft = &the_goldfish_timer;

	if (the_timesrc && the_schedtmr)
		return;

	gft->reg = (struct goldfish_timer_reg *)0xff003000;
	gft->timesrc.freq_hz = 1000000000;

	cprintf("Goldfish Timer: %" PRIu64 " Hz\n", gft->timesrc.freq_hz);

	irq_register(3, &goldfish_timer_ih);
/* XXX */
goldfish_alarm_setticks(gft, 1000000000);
	gft->timesrc.type = time_source_goldfish;
	gft->timesrc.arg = gft;
	gft->timesrc.ticks = &goldfish_timer_getticks;
	gft->timesrc.delay_nsec = &goldfish_timer_delay;
	the_timesrc = &gft->timesrc;

	gft->preempt.arg = gft;
	gft->preempt.schedule_nsec = &goldfish_timer_schedule;
	the_schedtmr = &gft->preempt;
}
