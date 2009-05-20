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

	int which;	/* which timer: MSM_TIMER_{GP,DG} */
	int shift;	/* timer hz shift */

	uint64_t total_ticks;
	uint32_t last_count;

	struct msm_timer_reg *reg;
};

#define TIMER_REG(_msmt, _reg)	_msmt->reg->timer[_msmt->which]._reg

static uint64_t
msm_timer_getticks(void *arg)
{
	struct msm_timer_state *msmt = arg;
	uint32_t cnt, newcnt;

	cnt = newcnt = TIMER_REG(msmt, count_val);
	if (cnt < msmt->last_count)
		newcnt = 0xffffffff - msmt->last_count + cnt;
	else
		newcnt = cnt - msmt->last_count;
	msmt->last_count = cnt;

	msmt->total_ticks += (newcnt >> msmt->shift);
	return (msmt->total_ticks);
}

static void
msm_alarm_setticks(void *arg, uint64_t now, uint64_t then)
{
	struct msm_timer_state *msmt = arg;
	uint32_t delta;

	assert(then >= now);
	assert(((then - now) << msmt->shift) <= 0xffffffff);

	delta = then - now;
	if (now + delta < now)
		then -= (0xffffffff - now);

	TIMER_REG(msmt, match_val) = then << msmt->shift;
}

static void
msm_timer_schedule(void *arg, uint64_t nsec)
{
	struct msm_timer_state *msmt = arg;
	uint64_t ticks;

	ticks = timer_convert(nsec, msmt->timesrc.freq_hz, 1000000000);

	uint64_t now = msm_timer_getticks(msmt);
	uint64_t then = now + ticks;
	msm_alarm_setticks(msmt, now, then);

	now = msm_timer_getticks(msmt);
	int64_t diff = then - now;
	if (diff < 0) {
		msm_alarm_setticks(msmt, now, now + msmt->timesrc.freq_hz);
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

	// update global tick counter to prevent 32-bit register saturation
	msm_timer_getticks(msmt);

	schedule();
}

void
msm_timer_init(uint32_t base, int irq, int which, uint64_t freq_hz)
{
	static struct msm_timer_state the_msm_timer;
	static struct interrupt_handler msm_timer_ih = {
		.ih_func = &msm_timer_intr,
		.ih_arg = &the_msm_timer
	};
	struct msm_timer_state *msmt = &the_msm_timer;

	if (the_timesrc && the_schedtmr)
		return;

	assert(which == MSM_TIMER_GP || which == MSM_TIMER_DG);

	msmt->which = which;
	if (msmt->which == MSM_TIMER_GP)
		msmt->shift = MSM_TIMER_GP_HZ_SHIFT;
	else
		msmt->shift = MSM_TIMER_DG_HZ_SHIFT;

	msmt->reg = (struct msm_timer_reg *)base;
	msmt->timesrc.freq_hz = freq_hz >> msmt->shift;

	cprintf("MSM %s Timer @ 0x%08x, irq %d, %" PRIu64 " Hz\n",
	    (which == MSM_TIMER_GP) ? "GP" : "DG", base, irq,
	    msmt->timesrc.freq_hz);

	TIMER_REG(msmt, clear) = 0;
	TIMER_REG(msmt, match_val) = 0xffffffff;
	TIMER_REG(msmt, enable) = ENABLE_EN;

	irq_register(irq, &msm_timer_ih);

	msmt->timesrc.type = time_source_msm;
	msmt->timesrc.arg = msmt;
	msmt->timesrc.ticks = &msm_timer_getticks;
	msmt->timesrc.delay_nsec = &msm_timer_delay;
	the_timesrc = &msmt->timesrc;

	msmt->preempt.arg = msmt;
	msmt->preempt.schedule_nsec = &msm_timer_schedule;
	the_schedtmr = &msmt->preempt;
}
