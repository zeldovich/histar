#include <kern/lib.h>
#include <kern/console.h>
#include <kern/intr.h>

#include <machine/leon3.h>
#include <machine/leon.h>
#include <machine/sparc-config.h>

#include <dev/amba.h>
#include <dev/ambapp.h>
#include <dev/apbucons.h>

enum { baud_rate = 38400 };

static void
serial_putc(void *arg, int c)
{
    if (c == '\n')
	serial_putc(arg, '\r');
    LEON3_APBUART_Regs_Map *uart_regs = (LEON3_APBUART_Regs_Map *)arg;
    
    uint32_t i = 0;
    while (!(LEON_BYPASS_LOAD_PA(&(uart_regs->status)) &
	     LEON_REG_UART_STATUS_THE) && (i < 12800))
	i++;
    
    LEON_BYPASS_STORE_PA(&(uart_regs->data), c);
    
    i = 0;
    while (!(LEON_BYPASS_LOAD_PA(&(uart_regs->status)) &
	     LEON_REG_UART_STATUS_TSE) && (i < 12800))
	i++;
}

static int
serial_proc_data(void *arg)
{
    LEON3_APBUART_Regs_Map *uart_regs = (LEON3_APBUART_Regs_Map *)arg;    
    uint32_t status = LEON_BYPASS_LOAD_PA(&(uart_regs->status));
    if (status & LEON_REG_UART_STATUS_DR) {
	uint32_t data = LEON_BYPASS_LOAD_PA(&(uart_regs->data));
	return data & 0xFF; 
    }
    return -1;
}

static void
serial_intr(void *arg)
{
    LEON3_APBUART_Regs_Map *uart_regs = (LEON3_APBUART_Regs_Map *)arg;
    cons_intr(serial_proc_data, uart_regs);
}

void
apbucons_init(void)
{    
    struct amba_apb_device dev;
    uint32_t r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_APBUART, &dev, 0);
    if (!r)
	return;
    
    /* Only register serial port A */
    if (dev.irq != LEON_INTERRUPT_UART_1_RX_TX) {
	r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_APBUART, &dev, 1);
	if (r && dev.irq != LEON_INTERRUPT_UART_1_RX_TX)
	    return;
    }

    uint32_t scaler = (CLOCK_FREQ_KHZ * 1000) / (baud_rate * 8);
    LEON3_APBUART_Regs_Map *uart_regs = (LEON3_APBUART_Regs_Map *)dev.start;
    LEON_BYPASS_STORE_PA(&(uart_regs->scaler), scaler);

    /* enable rx, tx, and rx interrupts */
    uint32_t ctrl = LEON_REG_UART_CTRL_RE | LEON_REG_UART_CTRL_TE |
	LEON_REG_UART_CTRL_RI;
    LEON_BYPASS_STORE_PA(&(uart_regs->ctrl), ctrl);
    
    static struct interrupt_handler ih = { .ih_func = &serial_intr };
    ih.ih_arg = (void *)dev.start;
    irq_register(dev.irq, &ih);

    static struct cons_device cd = {
	.cd_pollin = &serial_proc_data,
	.cd_output = &serial_putc,
    };

    cd.cd_arg = (void *)dev.start;
    cons_register(&cd);
}
