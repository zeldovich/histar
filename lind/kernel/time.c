#include <linux/time.h>
#include <asm/irq.h>
#include <linux/irqreturn.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

#include <archcall.h>
#include <kern/signal.h>

#define BILLION (1000 * 1000 * 1000)

/* get/set time */
static DEFINE_SPINLOCK(timer_spinlock);
static unsigned long long local_offset = 0;

/* timer_irq */
static const char real_time_clock = 0;
static unsigned long long prev_nsecs;
static long long delta;   		/* Deviation per interval */

static void 
timer_irq(void)
{
    unsigned long long ticks = 0;
    
    if (real_time_clock) {
	if(prev_nsecs) {
	    unsigned long long nsecs = arch_nsec();
	    
	    delta += nsecs - prev_nsecs;
	    prev_nsecs = nsecs;
	    
	    /* Protect against the host clock being set backwards */
	    if(delta < 0)
		delta = 0;
	    
	    ticks += (delta * HZ) / BILLION;
	    delta -= (ticks * BILLION) / HZ;
	}
	else 
	    prev_nsecs = arch_nsec();
    } else 
	ticks = 1;
    
    while(ticks > 0){
	__do_IRQ(LIND_TIMER_IRQ);
	ticks--;
    }
}

static void 
timer_handler(void)
{
    /* default to account to system time */
    int user_tick = 0;

    local_irq_disable();
    irq_enter();

    update_process_times(user_tick);
    irq_exit();
    local_irq_enable();
    
    timer_irq();
}

unsigned long long 
sched_clock(void) 
{
    return (unsigned long long)jiffies_64 * (BILLION / HZ);
}

static inline unsigned long long 
get_time(void)
{
    unsigned long long nsecs;
    unsigned long flags;
    
    spin_lock_irqsave(&timer_spinlock, flags);
    nsecs = arch_nsec();
    nsecs += local_offset;
    spin_unlock_irqrestore(&timer_spinlock, flags);
    
    return nsecs;
}

static inline void 
set_time(unsigned long long nsecs)
{
    unsigned long long now;
    unsigned long flags;
    
    spin_lock_irqsave(&timer_spinlock, flags);
    now = arch_nsec();
    local_offset = nsecs - now;
    spin_unlock_irqrestore(&timer_spinlock, flags);
    
    clock_was_set();
}

void 
do_gettimeofday(struct timeval *tv)
{
    unsigned long long nsecs = get_time();
    
    tv->tv_sec = nsecs / NSEC_PER_SEC;
    /* Careful about calculations here - this was originally done as
     * (nsecs - tv->tv_sec * NSEC_PER_SEC) / NSEC_PER_USEC
     * which gave bogus (> 1000000) values.  Dunno why, suspect gcc
     * (4.0.0) miscompiled it, or there's a subtle 64/32-bit conversion
     * problem that I missed.
     */
    nsecs -= tv->tv_sec * NSEC_PER_SEC;
    tv->tv_usec = (unsigned long) nsecs / NSEC_PER_USEC;
}

int 
do_settimeofday(struct timespec *tv)
{
    set_time((unsigned long long) tv->tv_sec * NSEC_PER_SEC + tv->tv_nsec);
    return 0;
}

irqreturn_t 
lind_timer(int irq, void *dev)
{
    unsigned long long nsecs;
    unsigned long flags;

    write_seqlock_irqsave(&xtime_lock, flags);
    
    do_timer(1);
    
    nsecs = get_time();
    xtime.tv_sec = nsecs / NSEC_PER_SEC;
    xtime.tv_nsec = nsecs - xtime.tv_sec * NSEC_PER_SEC;

    write_sequnlock_irqrestore(&xtime_lock, flags);
    
    return IRQ_HANDLED;
}

static void 
register_timer(void)
{
    int err;
    extern void (*sig_timer_handler)(void);
    
    err = request_irq(LIND_TIMER_IRQ, lind_timer, IRQF_DISABLED, "timer", NULL);
    if(err != 0)
	printk(KERN_ERR "register_timer : request_irq failed - "
	       "errno = %d\n", -err);
    sig_timer_handler = &timer_handler;
}

void
time_init(void)
{
    extern void (*late_time_init)(void);
    long long nsecs;

    nsecs = arch_nsec();
    set_normalized_timespec(&wall_to_monotonic, -nsecs / BILLION,
			    -nsecs % BILLION);
    late_time_init = register_timer;
}

