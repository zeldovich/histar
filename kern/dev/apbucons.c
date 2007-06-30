#include <kern/lib.h>
#include <kern/console.h>

#include <machine/leon3.h>
#include <machine/leon.h>
#include <machine/ambapp.h>

#include <dev/amba.h>
#include <dev/apbucons.h>

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
apbucons_init(void)
{    
    uint32_t irq;
    uint32_t reg_base = amba_find_apbslv_addr(VENDOR_GAISLER, 
					      GAISLER_APBUART, 
					      &irq);
    
    /* Only register serial port A */
    if (irq != LEON_INTERRUPT_UART_1_RX_TX) {
	struct amba_apb_device dev;
	int r = amba_find_next_apbslv_devices(VENDOR_GAISLER, GAISLER_APBUART, 
					      &dev, 1);
	if (r && dev.irq == LEON_INTERRUPT_UART_1_RX_TX)
	    reg_base = dev.start;
	else
	    return;
    }

    static struct cons_device cd = {
	.cd_pollin = 0,
	.cd_output = &serial_putc,
    };

    cd.cd_arg = (void *)reg_base;
    cons_register(&cd);
}
