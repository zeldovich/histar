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

#if 0
	while ((mtr->mtr_csr & MSM_UART_CSR_TX_READY) != 0)
		;
#else
	/* spin... */
	for (int i = 0; i < 4096; i++) {
		uint32_t tmp = mtr->mtr_csr;
		(void)tmp;
	}
#endif

	mtr->mtr_tfrf = c;
}

/*
 * Get data from the keyboard.  If we finish a character, return it.  Else 0.
 * Return -1 if no data.
 */
static int
msm_ttycons_proc_data(void *arg)
{
	struct msm_ttycons_reg *mtr = arg;
	uint32_t csr;

	if (mtr->mtr_csr & MSM_UART_CSR_OVERRUN) {
		cprintf("cons: OVERRUN detected\n");
		//mtr->mtr_cr = MSM_UART_CR_CMD_RESET_ERR;
	}

	if ((csr = mtr->mtr_csr) & MSM_UART_CSR_RX_READY) {
		if (csr & MSM_UART_CSR_RX_BREAK) {
			cprintf("cons: BREAK detected\n");
			return (0);
		}
		if (csr & MSM_UART_CSR_PAR_FRAME_ERR) {
			cprintf("cons: FRAME ERROR detected\n");
			return (0);
		}
	
		return (mtr->mtr_tfrf);
	}

	return (-1);
}

static void
msm_ttycons_intr(void *arg)
{
	cons_intr(msm_ttycons_proc_data, arg);
}

void
msm_ttycons_init(uint32_t base, int irq, int irq_rx)
{
	static struct cons_device msm_ttycons_cd = {
		.cd_pollin = &msm_ttycons_proc_data,
		.cd_output = &msm_ttycons_putc,
	};
	static struct interrupt_handler ih = {
		.ih_func = &msm_ttycons_intr
	};

	struct msm_ttycons_reg *mtr = (void *)base;
	msm_ttycons_cd.cd_arg = mtr;

	/* enable RX, CTS irqs */
if (0) {
//	mtr->mtr_imr = MSM_UART_IMR_RXLEV | MSM_UART_IMR_RXSTALE |
//	    MSM_UART_IMR_CURRENT_CTS;

	irq_register(irq, &ih);
	irq_register(irq_rx, &ih);
}

	cons_register(&msm_ttycons_cd);
}
