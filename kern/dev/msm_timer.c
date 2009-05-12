#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <kern/arch.h>
#include <kern/lib.h>
#include <dev/msm_timer.h>
#include <dev/msm_timerreg.h>

struct msm_timer_state {
	struct time_source timesrc;
	struct preemption_timer preempt;

	uint64_t total_ticks;
	uint32_t last_count;
	uint32_t last_alarm;

	struct msm_timer_reg *reg;
};

static uint64_t
msm_timer_getticks(void *arg)
{
	struct msm_timer_state *msmt = arg;
	uint32_t cnt;

	cnt = msmt->reg->mtr_countvalue;
	if (cnt < msmt->last_count)
		msmt->total_ticks += msmt->last_alarm - msmt->last_count + cnt;	
	else
		msmt->total_ticks += cnt - msmt->last_count;
	msmt->last_count = cnt;

	return (msmt->total_ticks);
}

static void
msm_alarm_setticks(void *arg, uint64_t ticks)
{
	struct msm_timer_state *msmt = arg;

	assert(ticks <= 0xffffffff);

	msmt->last_alarm = ticks;
	msmt->reg->mtr_matchvalue = ticks;
	msmt->reg->mtr_countvalue = 0;
	msmt->reg->mtr_enable = 1;
}

static void
msm_timer_schedule(void *arg, uint64_t nsec)
{
	struct msm_timer_state *msmt = arg;
	uint64_t ticks;

	ticks = timer_convert(nsec, msmt->timesrc.freq_hz, 1000000000);

	uint64_t now = msm_timer_getticks(msmt);
	uint64_t then = now + ticks;
	msm_alarm_setticks(msmt, then);

	now = msm_timer_getticks(msmt);
	int64_t diff = then - now;
	if (diff < 0) {
		msm_alarm_setticks(msmt, now + msmt->timesrc.freq_hz);
		cprintf("%s: lost a tick, recovering in a second\n", __func__);
	}
}

static void
msm_timer_delay(void *arg, uint64_t nsec)
{
	struct msm_timer_state *msmt = arg;
	uint64_t now = msm_timer_getticks(msmt); 
	uint64_t diff = timer_convert(nsec, msmt->timesrc.freq_hz, 1000000000);

	while ((msm_timer_getticks(msmt) - now) < diff)
		;
}

static void
msm_timer_intr(void *arg)
{
	struct msm_timer_state *msmt = arg;

	// update global tick counter
	msm_timer_getticks(msmt);
	
	// XXX no ack?
	schedule();
}

void
msm_timer_init(uint32_t base, int irq, uint64_t freq_hz)
{
	static struct msm_timer_state the_msm_timer;
	static struct interrupt_handler msm_timer_ih = {
		.ih_func = &msm_timer_intr,
		.ih_arg = &the_msm_timer
	};
	struct msm_timer_state *msmt = &the_msm_timer;

	if (the_timesrc && the_schedtmr)
		return;

	msmt->reg = (struct msm_timer_reg *)base;
	msmt->timesrc.freq_hz = freq_hz;

	cprintf("MSM Timer: %" PRIu64 " Hz\n", msmt->timesrc.freq_hz);

	msmt->reg->mtr_clear = 0;

	irq_register(irq, &msm_timer_ih);

/* XXX */
msm_alarm_setticks(msmt, 1000000000);
	msmt->timesrc.type = time_source_msm;
	msmt->timesrc.arg = msmt;
	msmt->timesrc.ticks = &msm_timer_getticks;
	msmt->timesrc.delay_nsec = &msm_timer_delay;
	the_timesrc = &msmt->timesrc;

	msmt->preempt.arg = msmt;
	msmt->preempt.schedule_nsec = &msm_timer_schedule;
	the_schedtmr = &msmt->preempt;
}
