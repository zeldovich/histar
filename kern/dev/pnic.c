#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <dev/pci.h>
#include <dev/pnic.h>
#include <dev/pnicreg.h>
#include <dev/kclock.h>
#include <kern/segment.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <kern/intr.h>
#include <kern/netdev.h>
#include <kern/arch.h>
#include <inc/queue.h>
#include <inc/netdev.h>
#include <inc/error.h>

#define PNIC_RX_SLOTS	64

struct pnic_rx_slot {
    uint16_t size;
    struct netbuf_hdr *nb;
    const struct Segment *sg;
};

struct pnic_card {
    uint32_t iobase;
    uint8_t irq_line;
    struct interrupt_handler ih;

    struct pnic_rx_slot rx[PNIC_RX_SLOTS];

    int rx_head;	// receive into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer

    struct net_device netdev;
};

static void
pnic_buffer_reset(struct pnic_card *c)
{
    for (int i = 0; i < PNIC_RX_SLOTS; i++) {
	if (c->rx[i].sg) {
	    kobject_unpin_page(&c->rx[i].sg->sg_ko);
	    pagetree_decpin(c->rx[i].nb);
	    kobject_dirty(&c->rx[i].sg->sg_ko);
	}
	c->rx[i].sg = 0;
    }

    c->rx_head = -1;
    c->rx_nextq = 0;
}

static void
pnic_buffer_reset_v(void *a)
{
    pnic_buffer_reset((struct pnic_card *) a);
}

static void
pnic_intr_enable(struct pnic_card *c)
{
    outw(c->iobase + PNIC_REG_LEN, 1);
    outb(c->iobase + PNIC_REG_DATA, 1);
    outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_MASK_IRQ);
}

static void
pnic_flush_read(struct pnic_card *c, uint16_t size)
{
    // Not so efficient, but whatever
    for (int i = 0; i < size; i++)
	inb(c->iobase + PNIC_REG_DATA);
}

static void
pnic_intr(void *arg)
{
    struct pnic_card *c = arg;

    for (;;) {
	outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_RECV_QLEN);
	uint16_t size = inw(c->iobase + PNIC_REG_LEN);
	if (size != 2) {
	    cprintf("pnic_intr: PNIC_CMD_RECV_QLEN response size %d\n", size);
	    break;
	}

	uint16_t qlen;
	insb(c->iobase + PNIC_REG_DATA, (void*)&qlen, 2);
	if (qlen == 0)
	    break;

	outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_RECV);
	size = inw(c->iobase + PNIC_REG_LEN);

	int i = c->rx_head;
	if (i == -1) {
	    //cprintf("pnic_intr: out of receive buffers\n");
	    pnic_flush_read(c, size);
	    continue;
	}

	if (size > c->rx[i].size) {
	    cprintf("pnic_intr: receive buffer too small: %d > %d\n",
		    size, c->rx[i].size);
	    pnic_flush_read(c, size);
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;
	} else {
	    void *buf = (c->rx[i].nb + 1);
	    insb(c->iobase + PNIC_REG_DATA, buf, size);
	    c->rx[i].nb->actual_count = size;
	}

	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	kobject_unpin_page(&c->rx[i].sg->sg_ko);
	pagetree_decpin(c->rx[i].nb);
	kobject_dirty(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;

	c->rx_head = (i + 1) % PNIC_RX_SLOTS;
	if (c->rx_head == c->rx_nextq)
	    c->rx_head = -1;
    }

    netdev_thread_wakeup(&c->netdev);
    pnic_intr_enable(c);
}

static int
pnic_add_txbuf(void *arg, const struct Segment *sg,
	       struct netbuf_hdr *nb, uint16_t size)
{
    struct pnic_card *c = arg;
    const char *buf = (const char *) (nb + 1);
    outw(c->iobase + PNIC_REG_LEN, size);
    outsb(c->iobase + PNIC_REG_DATA, buf, size);
    outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_XMIT);

    uint16_t stat = inw(c->iobase + PNIC_REG_STAT);
    if (stat != PNIC_STATUS_OK) {
	cprintf("pnic_add_txbuf: funny status %x\n", stat);
	nb->actual_count |= NETHDR_COUNT_ERR;
    }
    nb->actual_count |= NETHDR_COUNT_DONE;

    netdev_thread_wakeup(&c->netdev);
    return 0;
}

static int
pnic_add_rxbuf(void *arg, const struct Segment *sg,
	       struct netbuf_hdr *nb, uint16_t size)
{
    struct pnic_card *c = arg;
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    c->rx[slot].nb = nb;
    c->rx[slot].sg = sg;
    c->rx[slot].size = size;
    kobject_pin_page(&sg->sg_ko);
    pagetree_incpin(nb);

    c->rx_nextq = (slot + 1) % PNIC_RX_SLOTS;
    if (c->rx_head == -1)
	c->rx_head = slot;

    return 0;
}

int
pnic_attach(struct pci_func *pcif)
{
    struct pnic_card *c;
    int r = page_alloc((void **) &c);
    if (r < 0)
	return r;

    static_assert(PGSIZE >= sizeof(*c));
    memset(c, 0, sizeof(*c));

    pci_func_enable(pcif);
    if (pcif->reg_size[4] < 5) {
	cprintf("pnic_attach: io window too small %d\n", pcif->reg_size[4]);
	return 0;
    }

    c->irq_line = pcif->irq_line;
    c->iobase = pcif->reg_base[4];
    c->ih.ih_func = &pnic_intr;
    c->ih.ih_arg = c;

    pnic_buffer_reset(c);

    // Initialize the card
    outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_RESET);

    outw(c->iobase + PNIC_REG_CMD, PNIC_CMD_READ_MAC);
    uint16_t sz = inw(c->iobase + PNIC_REG_LEN);
    if (sz != 6) {
	cprintf("pnic_attach: MAC address size mismatch (%d)\n", sz);
	return 0;
    }
    insb(c->iobase + PNIC_REG_DATA, &c->netdev.mac_addr[0], 6);

    pnic_intr_enable(c);
    irq_register(c->irq_line, &c->ih);

    c->netdev.arg = c;
    c->netdev.add_buf_tx = &pnic_add_txbuf;
    c->netdev.add_buf_rx = &pnic_add_rxbuf;
    c->netdev.buffer_reset = &pnic_buffer_reset_v;

    // Register this card with the kernel
    netdev_register(&c->netdev);

    // All done
    cprintf("pnic: irq %d io 0x%x mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->irq_line, c->iobase,
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
    return 1;
}
