#include <machine/x86.h>
#include <kern/console.h>
#include <kern/arch.h>
#include <kern/intr.h>
#include <kern/lib.h>
#include <kern/prof.h>
#include <dev/sercons.h>
#include <dev/kbdreg.h>

// Stupid I/O delay routine necessitated by historical PC design flaws
static void
delay(void)
{
    inb(0x84);
    inb(0x84);
    inb(0x84);
    inb(0x84);
}

/***** Serial I/O code *****/

static struct serial_port {
    uint16_t io;
    uint8_t irq;
} serial_ports[] = {
    { 0x3f8, 4 },   /* COM1 */
    { 0x2f8, 3 },   /* COM2 */
};

#define COM_RX		0	// In:  Receive buffer (DLAB=0)
#define COM_TX		0	// Out: Transmit buffer (DLAB=0)
#define COM_DLL		0	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM		1	// Out: Divisor Latch High (DLAB=1)
#define COM_IER		1	// Out: Interrupt Enable Register
#define   COM_IER_RDI	0x01	//   Enable receiver data interrupt
#define COM_IIR		2	// In:  Interrupt ID Register
#define COM_FCR		2	// Out: FIFO Control Register
#define COM_LCR		3	// Out: Line Control Register
#define	  COM_LCR_DLAB	0x80	//   Divisor latch access bit
#define	  COM_LCR_WLEN8	0x03	//   Wordlength: 8 bits
#define COM_MCR		4	// Out: Modem Control Register
#define	  COM_MCR_RTS	0x02	// RTS complement
#define	  COM_MCR_DTR	0x01	// DTR complement
#define	  COM_MCR_OUT2	0x08	// Out2 complement
#define COM_LSR		5	// In:  Line Status Register
#define   COM_LSR_DATA	0x01	//   Data available
#define   COM_LSR_TXRDY	0x20	//   Transmit buffer avail
#define   COM_LSR_TSRE	0x40	//   Transmitter off

static int
serial_proc_data(void *arg)
{
    struct serial_port *com_port = arg;

    if (!(inb(com_port->io + COM_LSR) & COM_LSR_DATA))
	return -1;
    return inb(com_port->io + COM_RX);
}

static void
serial_intr(void *arg)
{
    struct serial_port *com_port = arg;
    cons_intr(serial_proc_data, com_port);
}

static void
serial_putc(void *arg, int c)
{
    struct serial_port *com_port = arg;

    for (int i = 0;
	 !(inb(com_port->io + COM_LSR) & COM_LSR_TXRDY) && i < 12800;
	 i++)
	delay();

    outb(com_port->io + COM_TX, c);
}

void
sercons_init(void)
{
    struct serial_port *com_port = &serial_ports[0];

    const char *s;
    if ((s = strstr(&boot_cmdline[0], "serial="))) {
	s += 7;
	if (!strncmp(s, "off", 3))
	    return;
	if (!strncmp(s, "com1", 4))
	    com_port = &serial_ports[0];
	if (!strncmp(s, "com2", 4))
	    com_port = &serial_ports[1];
    }

    // Turn off the FIFO
    outb(com_port->io + COM_FCR, 0);

    // Set speed; requires DLAB latch
    outb(com_port->io + COM_LCR, COM_LCR_DLAB);
    outb(com_port->io + COM_DLL, (uint8_t) (115200 / 9600));
    outb(com_port->io + COM_DLM, 0);

    // 8 data bits, 1 stop bit, parity off; turn off DLAB latch
    outb(com_port->io + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

    // No modem controls
    outb(com_port->io + COM_MCR, 0);
    // Enable rcv interrupts
    outb(com_port->io + COM_IER, COM_IER_RDI);

    // Clear any preexisting overrun indications and interrupts
    // Serial port doesn't exist if COM_LSR returns 0xFF
    bool_t serial_exists = (inb(com_port->io + COM_LSR) != 0xFF);
    (void) inb(com_port->io + COM_IIR);
    (void) inb(com_port->io + COM_RX);

    // Enable serial interrupts
    if (serial_exists) {
	static struct interrupt_handler ih = {.ih_func = &serial_intr };
	irq_register(com_port->irq, &ih);
    } else {
	cprintf("Serial port does not exist\n");
	return;
    }

    uint64_t start = karch_get_tsc();
    serial_putc(0, '\n');
    if (karch_get_tsc() - start > 0x100000) {
	cprintf("Serial port too slow, disabling.\n");
	return;
    }

    static struct cons_device cd = {
	.cd_pollin = &serial_proc_data,
	.cd_output = &serial_putc
    };

    cd.cd_arg = com_port;
    cons_register(&cd);
}
