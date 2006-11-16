#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <dev/pci.h>
#include <dev/fxp.h>
#include <dev/fxpreg.h>
#include <dev/kclock.h>
#include <kern/segment.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <kern/netdev.h>
#include <inc/queue.h>
#include <inc/netdev.h>
#include <inc/error.h>

#define FXP_RX_SLOTS	64
#define FXP_TX_SLOTS	64

struct fxp_tx_slot {
    struct fxp_cb_tx tcb;
    struct fxp_tbd tbd;
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

struct fxp_rx_slot {
    struct fxp_rfa rfd;
    struct fxp_rbd rbd;
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

// Static allocation ensures contiguous memory.
struct fxp_card {
    uint32_t iobase;
    uint8_t irq_line;
    uint16_t eeprom_width;
    struct interrupt_handler ih;

    struct fxp_tx_slot tx[FXP_TX_SLOTS];
    struct fxp_rx_slot rx[FXP_RX_SLOTS];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer
    bool_t rx_halted;	// receiver is not running

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer
    bool_t tx_halted;	// transmitter is not running and not suspended

    struct net_device netdev;
};

static struct fxp_card the_card;

static void
fxp_eeprom_shiftin(struct fxp_card *c, int data, int len)
{
    for (int x = 1 << (len - 1); x != 0; x >>= 1) {
	kclock_delay(40);
	uint16_t reg = ((data & x) ? FXP_EEPROM_EEDI : 0) | FXP_EEPROM_EECS;
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, reg);
	kclock_delay(40);
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
	kclock_delay(40);
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, reg);
    }
    kclock_delay(40);
}

static void
fxp_eeprom_autosize(struct fxp_card *c)
{
    outw(c->iobase + FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
    kclock_delay(40);

    fxp_eeprom_shiftin(c, FXP_EEPROM_OPC_READ, 3);
    int x;
    for (x = 1; x <= 8; x++) {
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	kclock_delay(40);
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS |
						FXP_EEPROM_EESK);
	kclock_delay(40);
	uint16_t v = inw(c->iobase + FXP_CSR_EEPROMCONTROL);
	if (!(v & FXP_EEPROM_EEDO))
	    break;
	kclock_delay(40);
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	kclock_delay(40);
    }

    outw(c->iobase + FXP_CSR_EEPROMCONTROL, 0);
    kclock_delay(40);

    c->eeprom_width = x;
}

static void
fxp_eeprom_read(struct fxp_card *c, uint16_t *buf, int off, int count)
{
    for (int i = 0; i < count; i++) {
	outw(c->iobase + FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(c, FXP_EEPROM_OPC_READ, 3);
	fxp_eeprom_shiftin(c, i + off, c->eeprom_width);

	uint16_t reg = FXP_EEPROM_EECS;
	buf[i] = 0;

	for (int x = 16; x > 0; x--) {
	    outw(c->iobase + FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
	    kclock_delay(40);
	    uint16_t v = inw(c->iobase + FXP_CSR_EEPROMCONTROL);
	    if ((v & FXP_EEPROM_EEDO))
		buf[i] |= (1 << (x - 1));
	    outw(c->iobase + FXP_CSR_EEPROMCONTROL, reg);
	    kclock_delay(40);
	}

	outw(c->iobase + FXP_CSR_EEPROMCONTROL, 0);
	kclock_delay(40);
    }
}

static void
fxp_scb_wait(struct fxp_card *c)
{
    for (int i = 0; i < 100000; i++) {
	uint8_t s = inb(c->iobase + FXP_CSR_SCB_COMMAND);
	if (s == 0)
	    return;
    }

    cprintf("fxp_scb_wait: timeout\n");
}

static void
fxp_scb_cmd(struct fxp_card *c, uint8_t cmd)
{
    outb(c->iobase + FXP_CSR_SCB_COMMAND, cmd);
}

static void
fxp_waitcomplete(volatile uint16_t *status)
{
    for (int i = 0; i < 1000; i++) {
	if ((*status & FXP_CB_STATUS_C))
	    return;
	kclock_delay(1);
    }

    cprintf("fxp_waitcomplete: timed out\n");
}

static void
fxp_buffer_reset(struct fxp_card *c)
{
    fxp_scb_wait(c);
    fxp_scb_cmd(c, FXP_SCB_COMMAND_RU_ABORT);

    // Wait for TX queue to drain
    for (;;) {
	int slot = c->tx_head;
	if (slot == -1)
	    break;

	for (int i = 0; i < 1000; i++) {
	    if ((c->tx[slot].tcb.cb_status & FXP_CB_STATUS_C))
		break;
	    kclock_delay(10);
	}

	c->tx_head = (slot + 1) % FXP_TX_SLOTS;
	if (c->tx_head == c->tx_nextq)
	    c->tx_head = -1;
    }

    for (int i = 0; i < FXP_TX_SLOTS; i++) {
	if (c->tx[i].sg) {
	    kobject_unpin_page(&c->tx[i].sg->sg_ko);
	    pagetree_decpin(c->tx[i].nb);
	    kobject_dirty(&c->tx[i].sg->sg_ko);
	}
	c->tx[i].sg = 0;
    }

    for (int i = 0; i < FXP_RX_SLOTS; i++) {
	if (c->rx[i].sg) {
	    kobject_unpin_page(&c->rx[i].sg->sg_ko);
	    pagetree_decpin(c->rx[i].nb);
	    kobject_dirty(&c->rx[i].sg->sg_ko);
	}
	c->rx[i].sg = 0;
    }

    c->rx_head = -1;
    c->rx_nextq = 0;
    c->rx_halted = 1;

    c->tx_head = -1;
    c->tx_nextq = 0;
    c->tx_halted = 1;
}

static void
fxp_buffer_reset_v(void *a)
{
    fxp_buffer_reset(a);
}

static int
fxp_rx_start(struct fxp_card *c)
{
    if (c->rx_head == -1) {
	cprintf("fxp_rx_start: no packets\n");
	return -1;
    }

    if (!c->rx_halted) {
	cprintf("fxp_rx_start: not halted\n");
	return -1;
    }

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, kva2pa(&c->rx[c->rx_head].rfd));
    fxp_scb_cmd(c, FXP_SCB_COMMAND_RU_START);
    c->rx_halted = 0;

    return 0;
}

static void
fxp_tx_start(struct fxp_card *c)
{
    if (c->tx_head == -1) {
	cprintf("fxp_tx_start: no packets\n");
	return;
    }

    if (!c->tx_halted) {
	cprintf("fxp_tx_start: not halted\n");
	return;
    }

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, kva2pa(&c->tx[c->tx_head].tcb));
    fxp_scb_cmd(c, FXP_SCB_COMMAND_CU_START);
    c->tx_halted = 0;
}

static void __attribute__((unused))
fxp_print_packet(struct netbuf_hdr *nb)
{
    cprintf("nb(%d/%d)", nb->actual_count & FXP_SIZE_MASK, nb->size);
    if ((nb->actual_count & NETHDR_COUNT_DONE))
	cprintf(" done");
    if ((nb->actual_count & NETHDR_COUNT_ERR))
	cprintf(" err");

    unsigned char *buf = (unsigned char*) (nb + 1);
    for (int i = 0; i < 25; i++)
	cprintf(" %02x", buf[i]);
    cprintf("\n");
}

static void
fxp_intr_rx(struct fxp_card *c)
{
    for (;;) {
	int i = c->rx_head;
	if (i == -1 || !(c->rx[i].rfd.rfa_status & FXP_RFA_STATUS_C))
	    break;

	kobject_unpin_page(&c->rx[i].sg->sg_ko);
	pagetree_decpin(c->rx[i].nb);
	kobject_dirty(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;
	c->rx[i].nb->actual_count = c->rx[i].rbd.rbd_count & FXP_SIZE_MASK;
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	if (!(c->rx[i].rfd.rfa_status & FXP_RFA_STATUS_OK))
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;

	c->rx_head = (i + 1) % FXP_RX_SLOTS;
	if (c->rx_head == c->rx_nextq)
	    c->rx_head = -1;
    }
}

static void
fxp_intr_tx(struct fxp_card *c)
{
    for (;;) {
	int i = c->tx_head;
	if (i == -1 || !(c->tx[i].tcb.cb_status & FXP_CB_STATUS_C))
	    break;

	kobject_unpin_page(&c->tx[i].sg->sg_ko);
	pagetree_decpin(c->tx[i].nb);
	kobject_dirty(&c->tx[i].sg->sg_ko);
	c->tx[i].sg = 0;
	c->tx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	c->tx_head = (i + 1) % FXP_TX_SLOTS;
	if (c->tx_head == c->tx_nextq)
	    c->tx_head = -1;
    }
}

static void
fxp_intr(void)
{
    struct fxp_card *c = &the_card;

    int r = inb(c->iobase + FXP_CSR_SCB_STATACK);
    outb(c->iobase + FXP_CSR_SCB_STATACK, r);

    if ((r & (FXP_SCB_STATACK_FR | FXP_SCB_STATACK_RNR)))
	fxp_intr_rx(c);
    if ((r & (FXP_SCB_STATACK_CXTNO | FXP_SCB_STATACK_CNA)))
	fxp_intr_tx(c);
    if ((r & FXP_SCB_STATACK_RNR)) {
	c->rx_halted = 1;
	if (fxp_rx_start(c) < 0) {
	    int s = inb(c->iobase + FXP_CSR_SCB_RUSCUS);
	    cprintf("fxp_intr: receiver stopped, stat %x ru/cu %x\n", r, s);
	}
    }

    netdev_thread_wakeup(&c->netdev);
}

static int
fxp_add_txbuf(struct fxp_card *c, const struct Segment *sg,
	      struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->tx_nextq;

    if (slot == c->tx_head)
	return -E_NO_SPACE;

    c->tx[slot].nb = nb;
    c->tx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    c->tx[slot].tbd.tb_addr = kva2pa(c->tx[slot].nb + 1);
    c->tx[slot].tbd.tb_size = size & FXP_SIZE_MASK;
    c->tx[slot].tcb.cb_status = 0;
    c->tx[slot].tcb.cb_command = FXP_CB_COMMAND_XMIT |
	FXP_CB_COMMAND_SF | FXP_CB_COMMAND_I | FXP_CB_COMMAND_S;

    int prev = (slot + FXP_TX_SLOTS - 1) % FXP_TX_SLOTS;
    c->tx[prev].tcb.cb_command &= ~FXP_CB_COMMAND_S;

    c->tx_nextq = (slot + 1) % FXP_TX_SLOTS;
    if (c->tx_head == -1)
	c->tx_head = slot;

    if (c->tx_halted) {
	fxp_tx_start(c);
    } else {
	fxp_scb_wait(c);
	fxp_scb_cmd(c, FXP_SCB_COMMAND_CU_RESUME);
    }

    return 0;
}

static int
fxp_add_rxbuf(struct fxp_card *c, const struct Segment *sg,
	      struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    c->rx[slot].nb = nb;
    c->rx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    c->rx[slot].rbd.rbd_buffer = kva2pa(c->rx[slot].nb + 1);
    c->rx[slot].rbd.rbd_size = size & FXP_SIZE_MASK;
    c->rx[slot].rfd.rfa_status = 0;
    c->rx[slot].rfd.rfa_control = FXP_RFA_CONTROL_SF | FXP_RFA_CONTROL_S;

    int prev = (slot + FXP_RX_SLOTS - 1) % FXP_RX_SLOTS;
    c->rx[prev].rfd.rfa_control &= ~FXP_RFA_CONTROL_S;

    c->rx_nextq = (slot + 1) % FXP_RX_SLOTS;
    if (c->rx_head == -1)
	c->rx_head = slot;

    if (c->rx_halted)
	fxp_rx_start(c);

    return 0;
}

static int
fxp_add_buf(void *a, const struct Segment *sg, uint64_t offset, netbuf_type type)
{
    struct fxp_card *c = a;
    uint64_t npage = offset / PGSIZE;
    uint32_t pageoff = PGOFF(offset);

    void *p;
    int r = kobject_get_page(&sg->sg_ko, npage, &p, page_excl_dirty);
    if (r < 0)
	return r;

    if (pageoff > PGSIZE || pageoff + sizeof(struct netbuf_hdr) > PGSIZE)
	return -E_INVAL;

    struct netbuf_hdr *nb = p + pageoff;
    uint16_t size = nb->size;
    if (pageoff + sizeof(struct netbuf_hdr) + size > PGSIZE)
	return -E_INVAL;

    if (type == netbuf_rx) {
	return fxp_add_rxbuf(c, sg, nb, size);
    } else if (type == netbuf_tx) {
	return fxp_add_txbuf(c, sg, nb, size);
    } else {
	return -E_INVAL;
    }
}

void
fxp_attach(struct pci_func *pcif)
{
    struct fxp_card *c = &the_card;
    memset(&c->netdev, 0, sizeof(c->netdev));

    if (pcif->reg_size[1] < 64) {
	cprintf("fxp_attach: io window too small: %d @ 0x%x\n",
		pcif->reg_size[1], pcif->reg_base[1]);
	return;
    }

    c->irq_line = pcif->irq_line;
    c->iobase = pcif->reg_base[1];
    c->ih.ih_func = &fxp_intr;

    for (int i = 0; i < FXP_TX_SLOTS; i++) {
	int next = (i + 1) % FXP_TX_SLOTS;
	memset(&c->tx[i], 0, sizeof(c->tx[i]));
	c->tx[i].tcb.link_addr = kva2pa(&c->tx[next].tcb);
	c->tx[i].tcb.tbd_array_addr = kva2pa(&c->tx[next].tbd);
	c->tx[i].tcb.tbd_number = 1;
	c->tx[i].tcb.tx_threshold = 4;
    }

    for (int i = 0; i < FXP_RX_SLOTS; i++) {
	int next = (i + 1) % FXP_RX_SLOTS;
	memset(&c->rx[i], 0, sizeof(c->rx[i]));
	c->rx[i].rfd.link_addr = kva2pa(&c->rx[next].rfd);
	c->rx[i].rfd.rbd_addr = kva2pa(&c->rx[i].rbd);
	c->rx[i].rbd.rbd_link = kva2pa(&c->rx[next].rbd);
    }

    c->rx_head = -1;
    c->tx_head = -1;
    fxp_buffer_reset(c);

    // Initialize the card
    outl(c->iobase + FXP_CSR_PORT, FXP_PORT_SOFTWARE_RESET);
    kclock_delay(50);

    uint16_t myaddr[3];
    fxp_eeprom_autosize(c);
    fxp_eeprom_read(c, &myaddr[0], 0, 3);
    for (int i = 0; i < 3; i++) {
	c->netdev.mac_addr[2*i + 0] = myaddr[i] & 0xff;
	c->netdev.mac_addr[2*i + 1] = myaddr[i] >> 8;
    }

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, 0);
    fxp_scb_cmd(c, FXP_SCB_COMMAND_CU_BASE);
    fxp_scb_wait(c);
    fxp_scb_cmd(c, FXP_SCB_COMMAND_RU_BASE);

    // Configure the card
    static struct fxp_cb_config cb_config;

    memset(&cb_config, 0, sizeof(cb_config));
    cb_config.cb_command = FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
    cb_config.byte_count = 8;
    cb_config.mediatype = 1;

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, kva2pa(&cb_config));
    fxp_scb_cmd(c, FXP_SCB_COMMAND_CU_START);
    fxp_waitcomplete(&cb_config.cb_status);

    // Program MAC address into the adapter
    static struct fxp_cb_ias cb_ias;

    cb_ias.cb_status = 0;
    cb_ias.cb_command = FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL;
    memcpy((void*)&cb_ias.macaddr[0], &c->netdev.mac_addr[0], 6);

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, kva2pa(&cb_ias));
    fxp_scb_cmd(c, FXP_SCB_COMMAND_CU_START);
    fxp_waitcomplete(&cb_ias.cb_status);

    // Register card with kernel
    irq_register(c->irq_line, &c->ih);

    c->netdev.arg = c;
    c->netdev.add_buf = &fxp_add_buf;
    c->netdev.buffer_reset = &fxp_buffer_reset_v;

    the_net_device = &c->netdev;

    // All done
    cprintf("fxp: irq %d io 0x%x mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->irq_line, c->iobase,
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
}
