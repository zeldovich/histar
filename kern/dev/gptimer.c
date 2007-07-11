#include <kern/lib.h>
#include <kern/timer.h>
#include <kern/intr.h>
#include <dev/gptimer.h>

enum { gpt_hz = 100 };

struct gpt_state {
    struct time_source gpt_src;
    uint64_t gpt_ticks;
};

static void
gpt_intr(void *arg)
{
    
}

static uint64_t
gpt_get_ticks(void *arg)
{
    return 0;
}

static void
gpt_delay(void *arg, uint64_t nsec)
{
}

static void
gpt_schedule(void *arg, uint64_t nsec)
{
}

void
gptimer_init(void)
{
    static struct gpt_state gpt_state;
    if (the_timesrc && the_schedtmr)
	return;

    static struct interrupt_handler gpt_ih =
	{ .ih_func = &gpt_intr, .ih_arg = 0 };
    /* XXX what IRQ num? */
    irq_register(0, &gpt_ih);
        
    gpt_state.gpt_src.type = time_source_gpt;
    gpt_state.gpt_src.freq_hz = gpt_hz;
    gpt_state.gpt_src.ticks = &gpt_get_ticks;
    gpt_state.gpt_src.delay_nsec = &gpt_delay;
    gpt_state.gpt_src.arg = &gpt_state;
    if (!the_timesrc)
	the_timesrc = &gpt_state.gpt_src;
    
    static struct preemption_timer gpt_preempt;
    gpt_preempt.schedule_nsec = &gpt_schedule;
    if (!the_schedtmr)
	the_schedtmr = &gpt_preempt;
    
    cprintf("XXX gptimer init\n");
}
