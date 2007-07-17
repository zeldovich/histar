#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <kern/sched.h>
#include <machine/leon3.h>
#include <machine/sparc-config.h>
#include <dev/gptimer.h>
#include <dev/ambapp.h>
#include <dev/amba.h>

struct gpt_ts {
    struct time_source gpt_src;
    uint32_t mask;
    uint32_t last_read;
    uint64_t ticks;
    LEON3_GpTimerElem_Regs_Map *regs;
};

struct gpt_sh {
    uint32_t hz;
    uint32_t mask;
    LEON3_GpTimerElem_Regs_Map *regs;
};

static uint32_t
gpt_tsval(struct gpt_ts *gpt)
{
    return gpt->mask - LEON_BYPASS_LOAD_PA(&(gpt->regs->val));
}

static uint64_t
gpt_get_ticks(void *arg)
{
    struct gpt_ts *gpt = (struct gpt_ts *)arg;
    uint32_t val = gpt_tsval(gpt);
    uint32_t diff = (val - gpt->last_read) & gpt->mask;
    gpt->last_read = val;
    gpt->ticks += diff;
    
    return gpt->ticks;
}

static void
gpt_delay(void *arg, uint64_t nsec)
{
    struct gpt_ts *gpt = (struct gpt_ts *)arg;
    uint32_t now = gpt_get_ticks(gpt);
    uint64_t diff = timer_convert(nsec, gpt->gpt_src.freq_hz, 1000000000);
    while ((gpt_get_ticks(gpt) - now) < diff)
	;
}

static void
gpt_schedule(void *arg, uint64_t nsec)
{
    struct gpt_sh *gpt = (struct gpt_sh *)arg;
    uint64_t ticks = timer_convert(nsec, gpt->hz, 1000000000);

    if (ticks > gpt->mask) {
	cprintf("gpt_schedule: ticks %"PRIu64" overflow timer\n", ticks);
	ticks = gpt->mask;
    }
    
    LEON_BYPASS_STORE_PA(&(gpt->regs->rld), (uint32_t)ticks);
    LEON_BYPASS_STORE_PA(&(gpt->regs->ctrl), 
			 LEON3_GPTIMER_EN | LEON3_GPTIMER_IRQEN | 
			 LEON3_GPTIMER_LD);
}

static void
gpt_intr(void *arg)
{
    LEON3_GpTimerElem_Regs_Map *sh_regs = (LEON3_GpTimerElem_Regs_Map *) arg;
    uint32_t ctrl = LEON_BYPASS_LOAD_PA(&(sh_regs->ctrl));
    ctrl &= ~LEON3_GPTIMER_CTRL_PENDING;
    LEON_BYPASS_STORE_PA(&(sh_regs->ctrl), ctrl);
    
    schedule();
}

void
gptimer_init(void)
{
    if (the_timesrc && the_schedtmr)
	return;

    struct amba_apb_device dev;
    uint32_t r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_GPTIMER, &dev, 0);
    if (!r)
	return;

    LEON3_GpTimer_Regs_Map *gpt_regs = (LEON3_GpTimer_Regs_Map *) dev.start;
    uint32_t conf = LEON_BYPASS_LOAD_PA(&(gpt_regs->config));
    uint32_t irq = GPTIMER_CONFIG_IRQNT(conf);
    
    /* 1 timer tick == 1 us */
    uint32_t hz = 1000000;
    uint32_t scalar_reload = (CLOCK_FREQ_KHZ / 1000) - 1;
    LEON_BYPASS_STORE_PA(&(gpt_regs->scalar_reload), scalar_reload);
    LEON_BYPASS_STORE_PA(&(gpt_regs->scalar), scalar_reload);
    
    /* Timer 0 for scheduler */
    LEON3_GpTimerElem_Regs_Map *sh_regs = 
	(LEON3_GpTimerElem_Regs_Map *) &(gpt_regs->e[0]);

    LEON_BYPASS_STORE_PA(&(sh_regs->rld), ~0);
    uint32_t mask = LEON_BYPASS_LOAD_PA(&(sh_regs->rld));

    static struct gpt_sh gpt_sh;
    gpt_sh.hz = hz;
    gpt_sh.regs = sh_regs;
    gpt_sh.mask = mask;
        
    static struct interrupt_handler gpt_ih;
    gpt_ih.ih_func = &gpt_intr;
    gpt_ih.ih_arg = sh_regs;
    
    static struct preemption_timer gpt_preempt;
    gpt_preempt.schedule_nsec = &gpt_schedule;
    gpt_preempt.arg = &gpt_sh;

    if (!the_schedtmr)
	the_schedtmr = &gpt_preempt;
    irq_register(irq, &gpt_ih);
    
    /* Timer 1 for time source */
    LEON3_GpTimerElem_Regs_Map *ts_regs = 
	(LEON3_GpTimerElem_Regs_Map *) &(gpt_regs->e[1]);

    LEON_BYPASS_STORE_PA(&(ts_regs->rld), ~0);
    LEON_BYPASS_STORE_PA(&(ts_regs->ctrl), 
			 LEON3_GPTIMER_EN | LEON3_GPTIMER_RL | LEON3_GPTIMER_LD);

    static struct gpt_ts gpt_ts;
    gpt_ts.mask = LEON_BYPASS_LOAD_PA(&(ts_regs->rld));
    gpt_ts.last_read = gpt_tsval(&gpt_ts);
    gpt_ts.ticks = 0;
    gpt_ts.regs = ts_regs;
    gpt_ts.gpt_src.type = time_source_gpt;
    gpt_ts.gpt_src.freq_hz = hz;
    gpt_ts.gpt_src.ticks = &gpt_get_ticks;
    gpt_ts.gpt_src.delay_nsec = &gpt_delay;
    gpt_ts.gpt_src.arg = &gpt_ts;
    if (!the_timesrc)
	the_timesrc = &gpt_ts.gpt_src;
}
