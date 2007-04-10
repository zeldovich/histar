#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <dev/pci.h>
#include <dev/e1000.h>
#include <dev/e1000reg.h>
#include <kern/segment.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <kern/netdev.h>
#include <kern/timer.h>
#include <kern/arch.h>
#include <inc/queue.h>
#include <inc/netdev.h>
#include <inc/error.h>

#define E1000_RX_SLOTS	64
#define E1000_TX_SLOTS	64

struct e1000_buffer_slot {
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

// Static allocation ensures contiguous memory.
struct e1000_tx_descs {
    struct wiseman_txdesc txd[E1000_TX_SLOTS] __attribute__((aligned (16)));
};

struct e1000_rx_descs {
    struct wiseman_rxdesc rxd[E1000_RX_SLOTS] __attribute__((aligned (16)));
};

struct e1000_card {
    uint32_t membase;
    uint32_t iobase;
    uint8_t irq_line;
    struct interrupt_handler ih;

    struct e1000_tx_descs *txds;
    struct e1000_rx_descs *rxds;

    struct e1000_buffer_slot tx[E1000_TX_SLOTS];
    struct e1000_buffer_slot rx[E1000_RX_SLOTS];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer

    struct net_device netdev;
};

static uint32_t
e1000_io_read(struct e1000_card *c, uint32_t reg)
{
    outl(c->iobase, reg);
    return inl(c->iobase + 4);
}

static void
e1000_io_write(struct e1000_card *c, uint32_t reg, uint32_t val)
{
    outl(c->iobase, reg);
    outl(c->iobase + 4, val);
}

static void
e1000_eeprom_read(struct e1000_card *c, uint16_t *buf, int off, int count)
{
    for (int i = 0; i < count; i++) {
	e1000_io_write(c, WMREG_EERD, ((off + i) << EERD_ADDR_SHIFT) | EERD_START);

	uint32_t reg;
	for (int x = 0; x < 100; x++) {
	    reg = e1000_io_read(c, WMREG_EERD);
	    if (!(reg & EERD_DONE))
		timer_delay(5000);
	}

	if (!(reg & EERD_DONE))
	    panic("e1000_eeprom_eerd_read: timeout");
	buf[i] = (reg & EERD_DATA_MASK) >> EERD_DATA_SHIFT;
    }
}

static void
e1000_reset(struct e1000_card *c)
{
    e1000_io_write(c, WMREG_RCTL, 0);
    e1000_io_write(c, WMREG_TCTL, 0);

    e1000_io_write(c, WMREG_PBA, PBA_48K);
    e1000_io_write(c, WMREG_CTRL, CTRL_RST);
    timer_delay(10 * 1000 * 1000);

    for (int i = 0; i < 1000; i++) {
	if ((e1000_io_read(c, WMREG_CTRL) & CTRL_RST) == 0)
	    break;
	timer_delay(20000);
    }

    if (e1000_io_read(c, WMREG_CTRL) & CTRL_RST)
	cprintf("e1000_reset: card still resetting, odd..\n");

    e1000_io_write(c, WMREG_CTRL, CTRL_SLU | CTRL_ASDE);

    uint64_t rptr = kva2pa(&c->rxds->rxd[0]);
    e1000_io_write(c, WMREG_RDBAH, rptr >> 32);
    e1000_io_write(c, WMREG_RDBAL, rptr & ((UINT64(1) << 32) - 1));
    e1000_io_write(c, WMREG_RDLEN, sizeof(c->rxds->rxd));
    e1000_io_write(c, WMREG_RDH, 0);
    e1000_io_write(c, WMREG_RDT, 0);
    e1000_io_write(c, WMREG_RDTR, 0);
    e1000_io_write(c, WMREG_RADV, 0);

    uint64_t tptr = kva2pa(&c->txds->txd[0]);
    e1000_io_write(c, WMREG_TBDAH, tptr >> 32);
    e1000_io_write(c, WMREG_TBDAL, tptr & ((UINT64(1) << 32) - 1));
    e1000_io_write(c, WMREG_TDLEN, sizeof(c->txds->txd));
    e1000_io_write(c, WMREG_TDH, 0);
    e1000_io_write(c, WMREG_TDT, 0);
    e1000_io_write(c, WMREG_TIDV, 1);
    e1000_io_write(c, WMREG_TADV, 1);

    // Disable VLAN
    e1000_io_write(c, WMREG_VET, 0);

    // Flow control junk?
    e1000_io_write(c, WMREG_FCAL, FCAL_CONST);
    e1000_io_write(c, WMREG_FCAH, FCAH_CONST);
    e1000_io_write(c, WMREG_FCT, 0x8808);
    e1000_io_write(c, WMREG_FCRTH, FCRTH_DFLT);
    e1000_io_write(c, WMREG_FCRTL, FCRTL_DFLT);
    e1000_io_write(c, WMREG_FCTTV, FCTTV_DFLT);

    // Interrupts
    e1000_io_write(c, WMREG_IMC, ~0);
    e1000_io_write(c, WMREG_IMS, ICR_TXDW | ICR_RXO | ICR_RXT0);

    // MAC address filters
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 0, (c->netdev.mac_addr[0]) |
						  (c->netdev.mac_addr[1] << 8) |
						  (c->netdev.mac_addr[2] << 16) |
						  (c->netdev.mac_addr[3] << 24));
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 4, (c->netdev.mac_addr[4]) |
						  (c->netdev.mac_addr[5] << 8) |
						  RAL_AV);
    for (int i = 2; i < WM_RAL_TABSIZE * 2; i++)
	e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + i * 4, 0);
    for (int i = 0; i < WM_MC_TABSIZE; i++)
	e1000_io_write(c, WMREG_CORDOVA_MTA + i * 4, 0);

    // Enable RX, TX
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
	if (i == -1 || !(c->rxds->rxd[i].wrx_status & WRX_ST_DD))
	    break;

	kobject_unpin_page(&c->rx[i].sg->sg_ko);
	pagetree_decpin(c->rx[i].nb);
	kobject_dirty(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;
	c->rx[i].nb->actual_count = c->rxds->rxd[i].wrx_len;
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	if (c->rxds->rxd[i].wrx_errors)
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
	if (i == -1 || !(c->txds->txd[i].wtx_fields.wtxu_status & WTX_ST_DD))
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
e1000_intr(void *arg)
{
    struct e1000_card *c = arg;
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

    c->txds->txd[slot].wtx_addr = kva2pa(c->tx[slot].nb + 1);
    c->txds->txd[slot].wtx_cmdlen = size | WTX_CMD_RS | WTX_CMD_EOP | WTX_CMD_IFCS;
    memset(&c->txds->txd[slot].wtx_fields, 0, sizeof(&c->txds->txd[slot].wtx_fields));

    c->tx_nextq = (slot + 1) % E1000_TX_SLOTS;
    if (c->tx_head == -1)
	c->tx_head = slot;

    e1000_io_write(c, WMREG_TDT, c->tx_nextq);
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

    memset(&c->rxds->rxd[slot], 0, sizeof(c->rxds->rxd[slot]));
    c->rxds->rxd[slot].wrx_addr = kva2pa(c->rx[slot].nb + 1);
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
    struct e1000_card *c;
    int r = page_alloc((void **) &c);
    if (r < 0) {
	cprintf("e1000_attach: cannot allocate memory: %s\n", e2s(r));
	return;
    }

    memset(c, 0, sizeof(*c));
    static_assert(PGSIZE >= sizeof(*c));
    static_assert(PGSIZE >= sizeof(*c->txds));
    static_assert(PGSIZE >= sizeof(*c->rxds));

    r = page_alloc((void **) &c->txds);
    if (r < 0) {
	cprintf("e1000_attach: cannot allocate txds: %s\n", e2s(r));
	return;
    }

    r = page_alloc((void **) &c->rxds);
    if (r < 0) {
	cprintf("e1000_attach: cannot allocate rxds: %s\n", e2s(r));
	return;
    }

    pci_func_enable(pcif);
    c->irq_line = pcif->irq_line;
    c->membase = pcif->reg_base[0];
    c->iobase = pcif->reg_base[2];

    // Get the MAC address
    uint16_t myaddr[3];
    e1000_eeprom_read(c, &myaddr[0], EEPROM_OFF_MACADDR, 3);
    for (int i = 0; i < 3; i++) {
	c->netdev.mac_addr[2*i + 0] = myaddr[i] & 0xff;
	c->netdev.mac_addr[2*i + 1] = myaddr[i] >> 8;
    }

    e1000_reset(c);

    // Register card with kernel
    c->ih.ih_func = &e1000_intr;
    c->ih.ih_arg = c;
    irq_register(c->irq_line, &c->ih);

    c->netdev.arg = c;
    c->netdev.add_buf = &e1000_add_buf;
    c->netdev.buffer_reset = &e1000_reset_v;
    netdev_register(&c->netdev);

    // All done
    cprintf("e1000: irq %d io 0x%x mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->irq_line, c->iobase,
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
}
