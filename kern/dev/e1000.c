#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <dev/pci.h>
#include <dev/e1000.h>
#include <dev/e1000reg.h>
#include <dev/kclock.h>
#include <kern/segment.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <kern/netdev.h>
#include <inc/queue.h>
#include <inc/netdev.h>
#include <inc/error.h>

#define E1000_RX_SLOTS	64
#define E1000_TX_SLOTS	64

struct e1000_tx_slot {
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

struct e1000_rx_slot {
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

// Static allocation ensures contiguous memory.
struct e1000_card {
    uint32_t membase;
    uint32_t iobase;
    uint8_t irq_line;
    uint8_t eeprom_spi : 1;
    uint16_t eeprom_width;
    struct interrupt_handler ih;

    struct wiseman_txdesc txd[E1000_TX_SLOTS] __attribute__((aligned (16)));
    struct e1000_tx_slot tx[E1000_TX_SLOTS];

    struct wiseman_rxdesc rxd[E1000_RX_SLOTS] __attribute__((aligned (16)));
    struct e1000_rx_slot rx[E1000_RX_SLOTS];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer

    struct net_device netdev;
};

static struct e1000_card the_card;

static uint32_t
e1000_io_read(struct e1000_card *c, uint32_t reg)
{
    outl(c->iobase, reg);
    return inl(c->iobase);
}

static void
e1000_io_write(struct e1000_card *c, uint32_t reg, uint32_t val)
{
    outl(c->iobase, reg);
    outl(c->iobase + 4, val);
}

#if 0
static void
e1000_eeprom_acquire(struct e1000_card *c)
{
    uint32_t reg = e1000_io_read(c, WMREG_EECD);

    reg |= EECD_EE_REQ;
    CSR_WRITE(sc, WMREG_EECD, reg);

    for (int x = 0; x < 100; x++) {
	reg = e1000_io_read(c, WMREG_EECD);
	if (reg & EECD_EE_GNT)
	    return;
	kclock_delay(5);
    }

    panic("e1000_eeprom_acquire: could not acquire EEPROM GNT\n");
}

static void
e1000_eeprom_release(struct e1000_card *c)
{
    e1000_io_write(c, WMREG_EECD,
		   e1000_io_read(c, WMREG_EECD) & ~EECD_EE_REQ);
}

static void
e1000_eeprom_read(struct e1000_card *c, uint16_t *buf, int off, int count)
{
    for (int i = 0; i < count; i++) {
	/* Clear SK and CS. */
	e1000_io_write(c, WMREG_EECD, e1000_io_read(c, WMREG_EECD) & ~(EECD_SK | EECD_CS));
	kclock_delay(2);

	XXX

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
#endif

static void
e1000_reset(struct e1000_card *c)
{
    e1000_io_write(c, WMREG_RCTL, 0);
    e1000_io_write(c, WMREG_TCTL, 0);

    e1000_io_write(c, WMREG_PBA, PBA_48K);
    e1000_io_write(c, WMREG_CTRL, CTRL_RST);
    kclock_delay(10 * 1000);

    for (int i = 0; i < 1000; i++) {
	if ((e1000_io_read(c, WMREG_CTRL) & CTRL_RST) == 0)
	    break;
	kclock_delay(20);
    }

    if (e1000_io_read(c, WMREG_CTRL) & CTRL_RST)
	cprintf("e1000_reset: card still resetting, odd..\n");

    uint64_t rptr = kva2pa(&c->rxd[0]);
    e1000_io_write(c, WMREG_RDBAH, rptr >> 32);
    e1000_io_write(c, WMREG_RDBAL, rptr & ((1UL << 32) - 1));
    e1000_io_write(c, WMREG_RDLEN, sizeof(c->rxd));
    e1000_io_write(c, WMREG_RDH, 0);
    e1000_io_write(c, WMREG_RDT, 0);
    e1000_io_write(c, WMREG_RDTR, 0);
    e1000_io_write(c, WMREG_RADV, 0);

    uint64_t tptr = kva2pa(&c->txd[0]);
    e1000_io_write(c, WMREG_TBDAH, tptr >> 32);
    e1000_io_write(c, WMREG_TBDAL, tptr & ((1UL << 32) - 1));
    e1000_io_write(c, WMREG_TDH, 0);
    e1000_io_write(c, WMREG_TDT, 0);
    e1000_io_write(c, WMREG_TIDV, 0);
    e1000_io_write(c, WMREG_TADV, 0);

    e1000_io_write(c, WMREG_RCTL, RCTL_EN | RCTL_RDMTS_1_2 | RCTL_DPF | RCTL_BAM);
    e1000_io_write(c, WMREG_TCTL,
		   TCTL_EN | TCTL_PSP | TCTL_CT(TX_COLLISION_THRESHOLD) |
		   TCTL_COLD(TX_COLLISION_DISTANCE_FDX));

    for (int i = 0; i < E1000_TX_SLOTS; i++) {
	if (c->tx[i].sg) {
	    kobject_unpin_page(&c->tx[i].sg->sg_ko);
	    pagetree_decpin(c->tx[i].nb);
	    kobject_dirty(&c->tx[i].sg->sg_ko);
	}
	c->tx[i].sg = 0;
    }

    for (int i = 0; i < E1000_RX_SLOTS; i++) {
	if (c->rx[i].sg) {
	    kobject_unpin_page(&c->rx[i].sg->sg_ko);
	    pagetree_decpin(c->rx[i].nb);
	    kobject_dirty(&c->rx[i].sg->sg_ko);
	}
	c->rx[i].sg = 0;
    }

    c->rx_head = -1;
    c->rx_nextq = 0;

    c->tx_head = -1;
    c->tx_nextq = 0;
}

static void
e1000_reset_v(void *a)
{
    e1000_reset(a);
}

static void
e1000_intr_rx(struct e1000_card *c)
{
    for (;;) {
	int i = c->rx_head;
	if (i == -1 || !(c->rxd[i].wrx_status & WRX_ST_DD))
	    break;

	kobject_unpin_page(&c->rx[i].sg->sg_ko);
	pagetree_decpin(c->rx[i].nb);
	kobject_dirty(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;
	c->rx[i].nb->actual_count = c->rxd[i].wrx_len;
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	if (c->rxd[i].wrx_errors)
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;

	c->rx_head = (i + 1) % E1000_RX_SLOTS;
	if (c->rx_head == c->rx_nextq)
	    c->rx_head = -1;
    }
}

static void
e1000_intr_tx(struct e1000_card *c)
{
    for (;;) {
	int i = c->tx_head;
	if (i == -1 || !(c->txd[i].wtx_fields.wtxu_status & WTX_ST_DD))
	    break;

	kobject_unpin_page(&c->tx[i].sg->sg_ko);
	pagetree_decpin(c->tx[i].nb);
	kobject_dirty(&c->tx[i].sg->sg_ko);
	c->tx[i].sg = 0;
	c->tx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	c->tx_head = (i + 1) % E1000_TX_SLOTS;
	if (c->tx_head == c->tx_nextq)
	    c->tx_head = -1;
    }
}

static void
e1000_intr(void)
{
    struct e1000_card *c = &the_card;
    uint32_t icr = e1000_io_read(c, WMREG_ICR);

    if (icr & ICR_TXDW)
	e1000_intr_tx(c);

    if (icr & ICR_RXT0)
	e1000_intr_rx(c);

    if (icr & ICR_RXO) {
	cprintf("e1000_intr: receiver overrun\n");
	e1000_reset(c);
    }

    netdev_thread_wakeup(&c->netdev);
}

static int
e1000_add_txbuf(struct e1000_card *c, const struct Segment *sg,
		struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->tx_nextq;

    if (slot == c->tx_head)
	return -E_NO_SPACE;

    if (size > 1522) {
	cprintf("e1000_add_txbuf: oversize buffer, %d bytes\n", size);
	return -E_INVAL;
    }

    c->tx[slot].nb = nb;
    c->tx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    c->txd[slot].wtx_addr = kva2pa(c->tx[slot].nb + 1);
    c->txd[slot].wtx_cmdlen = size | WTX_CMD_RS | WTX_CMD_EOP;
    memset(&c->txd[slot].wtx_fields, 0, sizeof(&c->txd[slot].wtx_fields));

    c->tx_nextq = (slot + 1) % E1000_TX_SLOTS;
    if (c->tx_head == -1)
	c->tx_head = slot;

    e1000_io_write(c, WMREG_TDT, slot);
    return 0;
}

static int
e1000_add_rxbuf(struct e1000_card *c, const struct Segment *sg,
	        struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    // The receive buffer size is hard-coded in the RCTL register as 2K.
    // However, we configure it to reject packets over 1522 bytes long.
    if (size < 1522) {
	cprintf("e1000_add_rxbuf: buffer too small, %d bytes\n", size);
	return -E_INVAL;
    }

    c->rx[slot].nb = nb;
    c->rx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    memset(&c->rxd[slot], 0, sizeof(c->rxd[slot]));
    c->rxd[slot].wrx_addr = kva2pa(c->rx[slot].nb + 1);
    e1000_io_write(c, WMREG_RDT, slot);

    c->rx_nextq = (slot + 1) % E1000_RX_SLOTS;
    if (c->rx_head == -1)
	c->rx_head = slot;

    return 0;
}

static int
e1000_add_buf(void *a, const struct Segment *sg, uint64_t offset, netbuf_type type)
{
    struct e1000_card *c = a;
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
	return e1000_add_rxbuf(c, sg, nb, size);
    } else if (type == netbuf_tx) {
	return e1000_add_txbuf(c, sg, nb, size);
    } else {
	return -E_INVAL;
    }
}

void
e1000_attach(struct pci_func *pcif)
{
    struct e1000_card *c = &the_card;
    memset(&c, 0, sizeof(c));

    c->irq_line = pcif->irq_line;
    c->membase = pcif->reg_base[0];
    c->iobase = pcif->reg_base[2];
    c->ih.ih_func = &e1000_intr;

    e1000_reset(c);

    // EEPROM
    uint32_t reg = e1000_io_read(c, WMREG_EECD);
    if (reg & EECD_EE_TYPE) {
	c->eeprom_spi = 1;
	c->eeprom_width = (reg & EECD_EE_ABITS) ? 16 : 8;
    } else {
	cprintf("Yo, Microwire EEPROM!\n");
	c->eeprom_width = (reg & EECD_EE_ABITS) ? 8 : 6;
    }

#if 0
    uint16_t myaddr[3];
    fxp_eeprom_autosize(c);
    fxp_eeprom_read(c, &myaddr[0], 0, 3);
    for (int i = 0; i < 3; i++) {
	c->netdev.mac_addr[2*i + 0] = myaddr[i] & 0xff;
	c->netdev.mac_addr[2*i + 1] = myaddr[i] >> 8;
    }
#endif
    c->netdev.mac_addr[0] = 0x00;
    c->netdev.mac_addr[1] = 0x00;
    c->netdev.mac_addr[2] = 0x00;
    c->netdev.mac_addr[3] = 0xab;
    c->netdev.mac_addr[4] = 0xcd;
    c->netdev.mac_addr[5] = 0xee;

    // Disable VLAN
    e1000_io_write(c, WMREG_VET, 0);

    // Flow control junk?
    e1000_io_write(c, WMREG_FCAL, FCAL_CONST);
    e1000_io_write(c, WMREG_FCAH, FCAH_CONST);
    e1000_io_write(c, WMREG_FCT, 0x8808);
    e1000_io_write(c, WMREG_FCRTH, FCRTH_DFLT);
    e1000_io_write(c, WMREG_FCRTL, FCRTL_DFLT);
    e1000_io_write(c, WMREG_FCTTV, FCTTV_DFLT);

    // Configure the card
    e1000_io_write(c, WMREG_CTRL, 0);
    e1000_io_write(c, WMREG_IMC, ~0);
    e1000_io_write(c, WMREG_IMS, ICR_TXDW | ICR_RXO | ICR_RXT0);
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 0, (c->netdev.mac_addr[0]) |
						  (c->netdev.mac_addr[1] << 8) |
						  (c->netdev.mac_addr[2] << 16) |
						  (c->netdev.mac_addr[3] << 24));
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 4, (c->netdev.mac_addr[4]) |
						  (c->netdev.mac_addr[5] << 8));

    // Register card with kernel
    irq_register(c->irq_line, &c->ih);

    c->netdev.arg = c;
    c->netdev.add_buf = &e1000_add_buf;
    c->netdev.buffer_reset = &e1000_reset_v;

    the_net_device = &c->netdev;

    // All done
    cprintf("e1000: irq %d io 0x%x mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->irq_line, c->iobase,
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
}
