#include <linux/kernel.h>

#include <sysdep/barrier.h>
#include <archcall.h>
#include <kern/time.h>

static volatile int signals_enabled = 1;
static volatile int pending = 0;

void (*sig_timer_handler)(void);
void (*sig_eth_handler)(void);
void (*sig_netd_handler)(void);
void (*sig_kcall_handler)(void);

void
block_signals(void)
{
    signals_enabled = 0;
    mb();
}

void
unblock_signals(void)
{
    if (signals_enabled)
	return;

    // XXX assumes no ints during handlers
    if (pending) {
	int save_pending = pending;
	pending = 0;

	if ((save_pending & SIGNAL_ALARM) && sig_timer_handler)
	    sig_timer_handler();
	if ((save_pending & SIGNAL_ETH) && sig_eth_handler)
	    sig_eth_handler();
	if ((save_pending & SIGNAL_NETD) && sig_netd_handler)
	    sig_netd_handler();
	if ((save_pending & SIGNAL_KCALL) && sig_kcall_handler)
	    sig_kcall_handler();
    }
    
    signals_enabled = 1;
    mb();
}

int 
get_signals(void)
{
    return signals_enabled;
}

int
set_signals(int enable) 
{
    int ret = signals_enabled;

    if (enable)
	unblock_signals();
    else
	block_signals();

    return ret;
}

static void
signal_handler(signal_t s)
{
    int enabled = signals_enabled;
    
    if (!signals_enabled) {
	pending |= s;
	return;
    }

    block_signals();
    switch(s) {
    case SIGNAL_ALARM:
	if (sig_timer_handler)
	    sig_timer_handler();
	break;
    case SIGNAL_ETH:
	if (sig_eth_handler)
	    sig_eth_handler();
	break;
    case SIGNAL_NETD:
	if (sig_netd_handler)
	    sig_netd_handler();
	break;
    case SIGNAL_KCALL:
	if (sig_kcall_handler)
	    sig_kcall_handler();
    }
    set_signals(enabled);
}

void
lind_signal_init(void)
{
    arch_signal_handler(signal_handler);
}
