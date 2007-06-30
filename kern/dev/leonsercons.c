#include <kern/lib.h>
#include <kern/console.h>

#include <machine/leon3.h>
#include <machine/leon.h>
#include <machine/ambapp.h>

#include <dev/leonsercons.h>

static void
serial_putc(void *arg, int c)
{
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

void
leonsercons_init(uint32_t amba_conf, uint32_t reg_base)
{    
    /* Only register serial port A */
    if (AMBA_CONF_IRQ(amba_conf) != LEON_INTERRUPT_UART_1_RX_TX)
	return;
    
    static struct cons_device cd = {
	.cd_pollin = 0,
	.cd_output = &serial_putc,
    };

    cd.cd_arg = (void *)reg_base;
    cons_register(&cd);
}
