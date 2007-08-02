#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/netdev.h>
#include <kern/timer.h>
#include <kern/kobj.h>
#include <dev/greth.h>
#include <dev/grethreg.h>
#include <dev/mii.h>
#include <dev/ambapp.h>
#include <dev/amba.h>
#include <inc/error.h>

static const char greth_mac[6] = { 0x00, 0x5E, 0x00, 0x00, 0x00, 0x01 };

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

    struct greth_txbds *txbds;
    struct greth_rxbds *rxbds;
    
    struct greth_buffer_slot tx[GRETH_TXBD_NUM];
    struct greth_buffer_slot rx[GRETH_RXBD_NUM];

    uint32_t rx_head;	// card receiving into rx_head, -1 if none
    uint32_t rx_nextq;	// next slot for rx buffer

    uint32_t tx_head;	// card transmitting from tx_head, -1 if none
    uint32_t tx_nextq;	// next slot for tx buffer
};

static void
greth_set_mac(struct greth_regs *regs, const char *mac)
{
    int msb = mac[0] << 8 | mac[1];
    int lsb = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
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

    regs->control |= GRETH_RESET;
    
    for (int i = 0; i < 1000; i++) {
	if (!(regs->control & GRETH_RESET))
	    break;
	timer_delay(20000);
    }

    if (regs->control & GRETH_RESET)
	cprintf("greth_reset: card still resetting, odd..\n");

    /* interrupts, enable RX, TX */
    regs->control |= (GRETH_INT_RX | GRETH_INT_TX);
    regs->control |= (GRETH_RXEN | GRETH_TXEN);
    /* setup buffer descriptor pointers */
    regs->tx_desc_p = kva2pa(c->txbds);
    regs->rx_desc_p = kva2pa(c->rxbds);

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
    c->rx_nextq = 0;

    c->netdev.waiter_id = 0;
    netdev_thread_wakeup(&c->netdev);
}

static int
greth_add_buf(void *a, const struct Segment *sg, uint64_t offset, netbuf_type type)
{
    return -E_INVAL;
}

static void
greth_buffer_reset(void *a)
{
    greth_reset(a);
}

int
greth_init(void)
{
    static_assert(sizeof(struct greth_card) <= PGSIZE);

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
    greth_set_mac(regs, greth_mac);
    memcpy(&c->netdev.mac_addr[0], &greth_mac[0], 6);
    
    greth_reset(c);
  
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
