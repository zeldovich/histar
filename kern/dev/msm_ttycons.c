#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <kern/timer.h>
#include <inc/intmacro.h>
#include <dev/msm_ttycons.h>
#include <dev/msm_ttyconsreg.h>

enum { polling = 1 };
static struct msm_ttycons_reg *g_mtr;

static void
msm_ttycons_putc(void *arg, int c, cons_source src)
{
	struct msm_ttycons_reg *mtr = arg;
	int i = 0;

	// quick, ugly, misplaced kludge to make things nicer for `cu'
	if (c == '\n')
		msm_ttycons_putc(arg, '\r', src);

	while ((mtr->uart_sr & UART_SR_TXRDY) == 0) {
		if (i++ == 100000)
			panic("%s:%s: stuck\n", __FILE__, __func__);
	}

	mtr->uart_tf = c;
}

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
msm_ttycons_proc_data(void *arg)
{
	struct msm_ttycons_reg *mtr = arg;
	uint32_t sr;
	int ret = -1;

	if (mtr->uart_sr & UART_SR_UART_OVERRUN) {
		cprintf("cons: OVERRUN detected\n");
		mtr->uart_cr = UART_CR_CHANNEL_COMMAND_RESET_ERR_STAT;
	}

	if ((sr = mtr->uart_sr) & UART_SR_RXRDY) {
		if (sr & UART_SR_RX_BREAK) {
			cprintf("cons: BREAK detected\n");
			mtr->uart_cr = UART_CR_CHANNEL_COMMAND_RESET_BREAK;
			return (0);
		}
		if (sr & UART_SR_PAR_FRAME_ERR) {
			cprintf("cons: FRAME ERROR detected\n");
			return (0);
		}

		ret = mtr->uart_rf;
	}

	// out of nowhere i started getting these...
	if (ret == 0x11 || ret == 0x13)
		ret = -1;

	return (ret);
}

static void
msm_ttycons_intr(void *arg)
{
	cons_intr(msm_ttycons_proc_data, arg);
}

static void
msm_ttycons_poll()
{
	cons_intr(msm_ttycons_proc_data, g_mtr);
}

void
msm_ttycons_init(uint32_t base, int irq)
{
	static struct cons_device msm_ttycons_cd = {
		.cd_pollin = &msm_ttycons_proc_data,
		.cd_output = &msm_ttycons_putc,
	};
	static struct interrupt_handler ih = {
		.ih_func = &msm_ttycons_intr,
		.ih_arg = (void *)0xdeadbeef,
	};

	g_mtr = (void *)base;
	msm_ttycons_cd.cd_arg = ih.ih_arg = g_mtr;

	// only rx irq's
	g_mtr->uart_imr = UART_IMR_RXLEV;

	// interrupt when > 0 chars in the fifo
	g_mtr->uart_rfwr = 0;

	// character mode: error bits only for next guy in fifo
	g_mtr->uart_mr2 &= ~UART_MR2_ERROR_MODE;

	cons_register(&msm_ttycons_cd);

	cprintf("MSM serial console @ 0x%08x, irq %d, rfwr %d, tfwr %d%s\n",
	    base, irq, g_mtr->uart_rfwr, g_mtr->uart_tfwr,
	    (polling) ? " (POLLED)" : "");

	// I keep getting spurious IRQs and can't figure out why.
	// Screw it. We'll just poll the sucker.
	if (polling) {
	    static struct periodic_task msm_ttycons_timer;
	    msm_ttycons_timer.pt_interval_msec = 25;
	    msm_ttycons_timer.pt_fn = msm_ttycons_poll;
	    timer_add_periodic(&msm_ttycons_timer);
	} else {
		irq_register(irq, &ih);
	}
}
