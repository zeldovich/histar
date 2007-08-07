#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/netdev.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <dev/greth.h>
#include <dev/grethreg.h>
#include <dev/mii.h>
#include <dev/ambapp.h>
#include <dev/amba.h>
#include <inc/error.h>

struct greth_buffer_slot {
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

struct greth_txbds {
    struct greth_bd txbd[GRETH_TXBD_NUM] __attribute__((aligned (1024)));
};

struct greth_rxbds {
    struct greth_bd rxbd[GRETH_RXBD_NUM] __attribute__((aligned (1024)));
};

struct greth_card {
    struct greth_regs *regs;
    struct net_device netdev;
    uint8_t irq_line;
    struct interrupt_handler ih;
    
    struct greth_txbds *txbds;
    struct greth_rxbds *rxbds;
    
    struct greth_buffer_slot tx[GRETH_TXBD_NUM];
    struct greth_buffer_slot rx[GRETH_RXBD_NUM];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer
};

static void
greth_set_mac(struct greth_regs *regs, const char *mac)
{
    uint32_t msb = mac[0] << 8 | mac[1];
    uint32_t lsb = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
    regs->esa_msb = msb;
    regs->esa_lsb = lsb;
    /* Make sure GRETH likes the mac */
    assert(regs->esa_msb == msb && regs->esa_lsb == lsb);
}

static int
greth_wait_mii(struct greth_regs *regs)
{
    for (int i = 0; regs->mdio & GRETH_MII_BUSY; i++) {
	if (i == 1000)
	    return -E_BUSY;
	timer_delay(20000);
    }
    return 0;
}

static int
greth_read_mii(char *nibble, int regaddr, struct greth_regs *regs)
{
    /* assumes MDIO_PHYADDR is correct */
    int r;
    if ((r = greth_wait_mii(regs)) < 0)
	return r;
    
    regs->mdio = ((regaddr & MDIO_REGADDR_MASK) << MDIO_REGADDR_SHIFT) |
	MDIO_RD_BIT;

    if ((r = greth_wait_mii(regs)) < 0)
	return r;
        
    if (regs->mdio & GRETH_MII_NVALID)
	return -E_INVAL;
    
    *nibble = (regs->mdio >> MDIO_DATA_SHIFT) & MDIO_DATA_MASK;
    return 0;
}

static int
greth_write_mii(char nibble, int regaddr, struct greth_regs *regs)
{
    /* assumes MDIO_PHYADDR is correct */
    int r;
    if ((r = greth_wait_mii(regs)) < 0)
	return r;
    
    regs->mdio = ((nibble & MDIO_DATA_MASK) << MDIO_DATA_SHIFT) | 
	((regaddr & MDIO_REGADDR_MASK) << MDIO_REGADDR_SHIFT) |
	MDIO_WR_BIT;

    if ((r = greth_wait_mii(regs)) < 0)
	return r;
        
    if (regs->mdio & GRETH_MII_NVALID)
	return -E_INVAL;
    
    return 0;
}

static void
greth_reset(struct greth_card *c)
{
    struct greth_regs *regs = c->regs;

    /* reset PHY, and card */
    char phyctrl = 0;
    int r = greth_read_mii(&phyctrl, MII_BMCR, regs);
    if (r < 0)
	cprintf("greth_rest: mii read error %s\n", e2s(r));

    r = greth_write_mii(phyctrl | BMCR_RESET,  MII_BMCR, regs);
    if (r < 0)
	cprintf("greth_reset: mii write error %s\n", e2s(r));

    regs->control = GRETH_CTRL_RESET;

    for (int i = 0; i < 1000; i++) {
	if (!(regs->control & GRETH_CTRL_RESET))
	    break;
	timer_delay(20000);
    }

    if (regs->control & GRETH_CTRL_RESET)
	cprintf("greth_reset: card still resetting, odd..\n");

    /* enable interrupts */
    regs->control |= (GRETH_CTRL_RX_INT | GRETH_CTRL_TX_INT);

    /* setup buffer descriptor pointers */
    regs->tx_desc_p = kva2pa(c->txbds);
    regs->rx_desc_p = kva2pa(c->rxbds);

    /* clear enable bits just in case */
    memset(c->txbds, 0, sizeof(*c->txbds));
    memset(c->rxbds, 0, sizeof(*c->rxbds));

    /* clear all status bits */
    regs->status = ~0;

    for (int i = 0; i < GRETH_TXBD_NUM; i++) {
	if (c->tx[i].sg) {
	    kobject_unpin_page(&c->tx[i].sg->sg_ko);
	    pagetree_decpin(c->tx[i].nb);
	    kobject_dirty(&c->tx[i].sg->sg_ko);
	}
	c->tx[i].sg = 0;
    }
    
    for (int i = 0; i < GRETH_RXBD_NUM; i++) {
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

    c->netdev.waiter_id = 0;
    netdev_thread_wakeup(&c->netdev);
}

static int
greth_add_txbuf(struct greth_card *c, const struct Segment *sg,
		struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->tx_nextq;

    if (slot == c->tx_head)
	return -E_NO_SPACE;

    if (size > 1522) {
	cprintf("greth_add_txbuf: oversize buffer, %d bytes\n", size);
	return -E_INVAL;
    }

    c->tx[slot].nb = nb;
    c->tx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    memset(&c->txbds->txbd[slot], 0, sizeof(c->txbds->txbd[slot]));
    c->txbds->txbd[slot].addr = kva2pa(c->tx[slot].nb + 1);
    c->txbds->txbd[slot].stat = GRETH_BD_EN | GRETH_BD_IE | (size & GRETH_BD_LEN);
    c->regs->control |= GRETH_CTRL_TX_EN;

    c->tx_nextq = (slot + 1) % GRETH_TXBD_NUM;
    if (c->tx_head == -1)
	c->tx_head = slot;

    return 0;
}

static int
greth_add_rxbuf(struct greth_card *c, const struct Segment *sg,
	        struct netbuf_hdr *nb, uint16_t size)
{
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    // The max receive buffer size is hard-coded in the bd as 2K.
    // However, we configure it to reject packets over 1522 bytes long.
    if (size < 1522) {
	cprintf("greth_add_rxbuf: buffer too small, %d bytes\n", size);
	return -E_INVAL;
    }

    c->rx[slot].nb = nb;
    c->rx[slot].sg = sg;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    memset(&c->rxbds->rxbd[slot], 0, sizeof(c->rxbds->rxbd[slot]));
    c->rxbds->rxbd[slot].addr = kva2pa(c->rx[slot].nb + 1);
    c->rxbds->rxbd[slot].stat = GRETH_BD_EN | GRETH_BD_IE | (size & GRETH_BD_LEN);
    c->regs->control |= GRETH_CTRL_RX_EN;

    c->rx_nextq = (slot + 1) % GRETH_RXBD_NUM;
    if (c->rx_head == -1)
	c->rx_head = slot;

    return 0;
}

static int
greth_add_buf(void *a, const struct Segment *sg, uint64_t offset, netbuf_type type)
{
    struct greth_card *c = a;
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
	return greth_add_rxbuf(c, sg, nb, size);
    } else if (type == netbuf_tx) {
	return greth_add_txbuf(c, sg, nb, size);
    } else {
	return -E_INVAL;
    }
}

static void
greth_buffer_reset(void *a)
{
    greth_reset(a);
}

static void
greth_intr_rx(struct greth_card *c)
{
    for (;;) {
	int i = c->rx_head;
	if (i == -1 || (c->rxbds->rxbd[i].stat & GRETH_BD_EN))
	    break;

	kobject_unpin_page(&c->rx[i].sg->sg_ko);
	pagetree_decpin(c->rx[i].nb);
	kobject_dirty(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;
	c->rx[i].nb->actual_count = (c->rxbds->rxbd[i].stat & GRETH_BD_LEN);
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	if (c->rxbds->rxbd[i].stat & GRETH_RXBD_ERR)
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;

	c->rx_head = (i + 1) % GRETH_RXBD_NUM;
	if (c->rx_head == c->rx_nextq)
	    c->rx_head = -1;
    }
    c->regs->status |= (GRETH_STAT_RX_INT | GRETH_STAT_RX_ERR);
}

static void
greth_intr_tx(struct greth_card *c)
{
    for (;;) {
	int i = c->tx_head;
	if (i == -1 || (c->txbds->txbd[i].stat & GRETH_BD_EN))
	    break;

	kobject_unpin_page(&c->tx[i].sg->sg_ko);
	pagetree_decpin(c->tx[i].nb);
	kobject_dirty(&c->tx[i].sg->sg_ko);
	c->tx[i].sg = 0;
	c->tx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	c->tx_head = (i + 1) % GRETH_TXBD_NUM;
	if (c->tx_head == c->tx_nextq)
	    c->tx_head = -1;
    }
    c->regs->status |= (GRETH_STAT_TX_INT | GRETH_STAT_TX_ERR);
}

static void
greth_intr(void *arg)
{
    struct greth_card *c = arg;
    uint32_t status = c->regs->status;

    if (status & GRETH_STAT_TX_INT)
	greth_intr_tx(c);

    if (status & GRETH_STAT_RX_INT)
	greth_intr_rx(c);

    if (status & GRETH_STAT_TX_AHBERR) {
	cprintf("greth_intr: ahb tx error\n");
	greth_reset(c);
    }

    if (status & GRETH_STAT_RX_AHBERR) {
	cprintf("greth_intr: ahb rx error\n");
	greth_reset(c);
    }

    netdev_thread_wakeup(&c->netdev);
}

int
greth_init(void)
{
    struct amba_apb_device dev;
    int r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_ETHMAC, &dev, 0);
    if (!r)
	return -E_NOT_FOUND;
    struct greth_regs *regs = pa2kva(dev.start);
    
    struct greth_card *c;
    r = page_alloc((void **) &c);
    if (r < 0)
	return r;
    memset(c, 0, PGSIZE);
    static_assert(PGSIZE >= sizeof(*c));
    static_assert(PGSIZE >= sizeof(*c->txbds));
    static_assert(PGSIZE >= sizeof(*c->rxbds));
    
    r = page_alloc((void **) &c->txbds);
    if (r < 0) {
	page_free(c);
	return r;
    }
    
    r = page_alloc((void **) &c->rxbds);
    if (r < 0) {
	page_free(c->txbds);
	page_free(c);
	return r;
    }

    c->regs = regs;
    c->irq_line = dev.irq;

    /* Derive the MAC address from the EDCL IP address */
    char greth_mac[6] = { 0x00, 0x5E, 0x00, 0x00, 0x00, 0x00 };
    uint32_t edcl_ip = regs->edcl_ip;
    memcpy(&greth_mac[2], &edcl_ip, 4);

    greth_set_mac(regs, &greth_mac[0]);
    memcpy(&c->netdev.mac_addr[0], &greth_mac[0], 6);

    greth_reset(c);

    /* Register card with kernel */
    c->ih.ih_func = &greth_intr;
    c->ih.ih_arg = c;
    irq_register(c->irq_line, &c->ih);
      
    c->netdev.arg = c;
    c->netdev.add_buf = &greth_add_buf;
    c->netdev.buffer_reset = &greth_buffer_reset;
    netdev_register(&c->netdev);
    
    cprintf("greth: mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
    
    return 0;
}
