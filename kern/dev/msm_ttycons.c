#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <inc/intmacro.h>
#include <dev/msm_ttycons.h>
#include <dev/msm_ttyconsreg.h>

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

	if (mtr->uart_sr & UART_SR_UART_OVERRUN) {
		cprintf("cons: OVERRUN detected\n");
		mtr->uart_cr = UART_CR_CHANNEL_COMMAND_RESET_ERR_STAT;
	}

	if ((sr = mtr->uart_sr) & UART_SR_RXRDY) {
		if (sr & UART_SR_RX_BREAK) {
			cprintf("cons: BREAK detected\n");
			return (0);
		}
		if (sr & UART_SR_PAR_FRAME_ERR) {
			cprintf("cons: FRAME ERROR detected\n");
			return (0);
		}

		return (mtr->uart_rf);
	}

	return (-1);
}

static void
msm_ttycons_intr(void *arg)
{
	cons_intr(msm_ttycons_proc_data, arg);
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

	struct msm_ttycons_reg *mtr = (void *)base;
	msm_ttycons_cd.cd_arg = ih.ih_arg = mtr;

	// only rx irq's
	mtr->uart_imr = UART_IMR_RXLEV;

	// interrupt when > 0 chars in the fifo
	mtr->uart_rfwr = 0;

	cons_register(&msm_ttycons_cd);

	cprintf("MSM serial console @ 0x%08x, irq %d, rfwr %d, tfwr %d\n",
	    base, irq, mtr->uart_rfwr, mtr->uart_tfwr);

	irq_register(irq, &ih);
}
