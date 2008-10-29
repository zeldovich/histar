#include <machine/x86.h>
#include <dev/ne2kpci.h>
#include <dev/dp8390reg.h>

#include <jdev/ne2kpci.h>
#include <inc/error.h>
#include <inc/arch.h>
#include <inc/syscall.h>
#include <inc/lib.h>

#include <assert.h>
#include <malloc.h>
#include <string.h>

#define NE2KPCI_RX_SLOTS 64

#define NE2KPCI_TXP_BUF	0x40
#define NE2KPCI_PSTART	0x46
#define NE2KPCI_PSTOP	0xAA	// "good" ring size?

#define NE2KPCI_IOPORT	0x10
#define NE2KPCI_RESET	0x1f

struct ne2kpci_rx_slot {
    uint16_t size;
    struct dp8390_ring rrd;
    struct netbuf_hdr *nb;
};


struct ne2kpci_card {
    struct cobj_ref obj;
    bool_t running;

    uint8_t irq_line;
    uint32_t iobase;

    struct ne2kpci_rx_slot rx[NE2KPCI_RX_SLOTS];

    int rx_head;		// receive into rx_head, -1 if none
    int rx_nextq;		// next slot for rx buffer
    uint8_t next_pkt;

    uint8_t mac_addr[6];

    void *seg_va;
    struct cobj_ref seg;
    uint64_t seg_bytes;
};

static void
ne2kpci_rmt_read(struct ne2kpci_card* c, uint8_t* buf,
		 uint16_t size, uint16_t ad)
{
    outb(c->iobase + ED_P0_RBCR0, size & 0x00FF);
    outb(c->iobase + ED_P0_RBCR1, (size >> 8) & 0x00FF);
    outb(c->iobase + ED_P0_RSAR0, ad & 0x00FF);
    outb(c->iobase + ED_P0_RSAR1, (ad >> 8) & 0x00FF);
    outb(c->iobase + ED_P0_CR, ED_CR_RD0 | ED_CR_STA);

    insb(c->iobase + NE2KPCI_IOPORT, buf, size);
}

static void
ne2kpci_rmt_write(struct ne2kpci_card *c, uint8_t *buf,
		  uint16_t size, uint16_t ad)
{
    outb(c->iobase + ED_P0_RBCR0, size & 0x00FF);
    outb(c->iobase + ED_P0_RBCR1, (size >> 8) & 0x00FF);
    outb(c->iobase + ED_P0_RSAR0, ad & 0x00FF);
    outb(c->iobase + ED_P0_RSAR1, (ad >> 8) & 0x00FF);
    outb(c->iobase + ED_P0_CR, ED_CR_RD1 | ED_CR_STA);

    outsb(c->iobase + NE2KPCI_IOPORT, buf, size);
}

static void
ne2kpci_flush_packet(struct ne2kpci_card *c, struct dp8390_ring *rrd)
{
    // update desribed in NS reference
    c->next_pkt = rrd->next_packet;
    uint8_t boundary = c->next_pkt - 1;
    if (boundary < NE2KPCI_PSTART)
	boundary = NE2KPCI_PSTOP - 1;
    outb(c->iobase + ED_P0_BNRY, boundary);
}

static void
ne2kpci_buffer_reset(struct ne2kpci_card* c)
{
    c->rx_head = -1;
    c->rx_nextq = 0;
}

static int
ne2kpci_reset(struct ne2kpci_card *c)
{
    int r;
    uint64_t iobase;
    r = sys_udev_get_base(c->obj, 0, &iobase);
    if (r < 0)
	return r;
    c->iobase = iobase;

    ne2kpci_buffer_reset(c);

    // init procedure as specified in NS reference

    // reset
    outb(c->iobase + NE2KPCI_RESET, inb(c->iobase + NE2KPCI_RESET));
    uint64_t tsc_start = arch_read_tsc();
    for (;;) {
	uint8_t isr = inb(c->iobase + ED_P0_ISR);
	if (isr & ED_ISR_RST)
	    break;

	uint64_t tsc_diff = arch_read_tsc() - tsc_start;
	if (tsc_diff > 1024 * 1024 * 1024) {
	    printf("ne2kpci_init: stuck for %"PRIu64" cycles, isr 0x%x\n",
		   tsc_diff, isr);
	    return -E_BUSY;
	}
    }

    outb(c->iobase + ED_P0_CR, ED_CR_STP | ED_CR_RD2);
    outb(c->iobase + ED_P0_DCR, ED_DCR_LS | ED_DCR_FT1);

    outb(c->iobase + ED_P0_RBCR0, 0);
    outb(c->iobase + ED_P0_RBCR1, 0);
    outb(c->iobase + ED_P0_RCR, ED_RCR_AB | ED_RCR_PRO);

    outb(c->iobase + ED_P0_TCR, ED_TCR_LB0);

    // initialize receive buffer ring.
    outb(c->iobase + ED_P0_BNRY, NE2KPCI_PSTART);
    outb(c->iobase + ED_P0_PSTART, NE2KPCI_PSTART);
    outb(c->iobase + ED_P0_PSTOP, NE2KPCI_PSTOP);

    outb(c->iobase + ED_P0_CR, ED_CR_PAGE_1 | ED_CR_RD2 | ED_CR_STP);
    outb(c->iobase + ED_P1_CURR, NE2KPCI_PSTART + 1);
    c->next_pkt = NE2KPCI_PSTART + 1;
    outb(c->iobase + ED_P0_CR, ED_CR_PAGE_0 | ED_CR_RD2 | ED_CR_STP);

    // interrupts
    outb(c->iobase + ED_P0_ISR, 0xFF);
    outb(c->iobase + ED_P0_IMR, (ED_IMR_PRXE | ED_IMR_RXEE |
				 ED_IMR_TXEE | ED_IMR_OVWE));

    // read station address
    uint8_t buf[12];
    ne2kpci_rmt_read(c, buf, 12, 0x00);
    for (int i = 0; i < 6; i++)
	c->mac_addr[i] = buf[i * 2];

    // set the station address
    outb(c->iobase + ED_P0_CR, ED_CR_PAGE_1 | ED_CR_RD2 | ED_CR_STP);
    for (int i = 0; i < 6; i++)
	outb(c->iobase + ED_P1_PAR0 + i, c->mac_addr[i]);

    // accept all multicast
    for (int i = 0; i < 8; i++)
	outb(c->iobase + ED_P1_MAR0 + i, 0xFF);
    outb(c->iobase + ED_P0_CR, ED_CR_PAGE_0 | ED_CR_RD2 | ED_CR_STA);

    outb(c->iobase + ED_P0_TCR, 0);
    return 0;
}

static int
ne2kpci_add_txbuf(void *arg, struct netbuf_hdr *nb, uint16_t size)
{
    struct ne2kpci_card *c = arg;

    // txp queue?
    if (inb(c->iobase + ED_P0_CR) & ED_CR_TXP) {
	printf("ne2kpci_add_txbuf: txp queue?");
	while (inb(c->iobase + ED_P0_CR) & ED_CR_TXP) ;
    }

    ne2kpci_rmt_write(c, (uint8_t *) (nb + 1), size,
		      NE2KPCI_TXP_BUF << ED_PAGE_SHIFT);

    outb(c->iobase + ED_P0_TPSR, NE2KPCI_TXP_BUF);
    outb(c->iobase + ED_P0_TBCR0, size & 0x00ff);
    outb(c->iobase + ED_P0_TBCR1, size >> 8);
    outb(c->iobase + ED_P0_CR, ED_CR_STA | ED_CR_TXP | ED_CR_RD2);

    nb->actual_count |= NETHDR_COUNT_DONE;
    return 0;
}

static int
ne2kpci_add_rxbuf(void *arg, struct netbuf_hdr *nb, uint16_t size)
{
    struct ne2kpci_card *c = arg;
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    c->rx[slot].nb = nb;
    c->rx[slot].size = size;

    c->rx_nextq = (slot + 1) % NE2KPCI_RX_SLOTS;
    if (c->rx_head == -1)
	c->rx_head = slot;

    return 0;
}

int
ne2kpci_macaddr(void* arg, uint8_t* macaddr)
{
    struct ne2kpci_card *c = arg;
    memcpy(macaddr, c->mac_addr, 6);
    return 0;
}

int
ne2kpci_buf(void* arg, struct cobj_ref seg,
	    uint64_t offset, netbuf_type type)
{
    int r;
    struct ne2kpci_card* c = arg;
    // XXX client can only use one segment
    if (!c->seg_va) {
	c->seg = seg;
	r = segment_map(c->seg, 0, SEGMAP_READ | SEGMAP_WRITE, 
			&c->seg_va, &c->seg_bytes, 0);
	if (r < 0)
	    return r;
    }
    
    struct netbuf_hdr* nb = c->seg_va + offset;
    uint16_t size = nb->size;
    
    switch (type) {
    case netbuf_rx:
	return ne2kpci_add_rxbuf(c, nb, size);
    case netbuf_tx:
	return ne2kpci_add_txbuf(c, nb, size);
    default:
	return -E_INVAL;
    }
}

static void
ne2kpci_stop(struct ne2kpci_card *c)
{
    outb(c->iobase + ED_P0_CR, ED_CR_PAGE_0 | ED_CR_STP);
    while ((inb(c->iobase + ED_P0_ISR) & ED_ISR_RST) == 0) ;
}

static void
ne2kpci_rintr(struct ne2kpci_card *c)
{
    for (;;) {
	outb(c->iobase + ED_P0_CR, ED_CR_PAGE_1 | ED_CR_STA);
	uint8_t current = inb(c->iobase + ED_P1_CURR);
	outb(c->iobase + ED_P0_CR, ED_CR_PAGE_0 | ED_CR_STA);

	if (c->next_pkt == current)
	    break;

	int i = c->rx_head;
	if (i == -1) {
	    struct dp8390_ring r;
	    ne2kpci_rmt_read(c, (uint8_t *) & r, 4,
			     c->next_pkt << ED_PAGE_SHIFT);
	    ne2kpci_flush_packet(c, &r);
	    continue;
	}

	struct dp8390_ring *rrd = &c->rx[i].rrd;
	void *buf = (c->rx[i].nb + 1);

	ne2kpci_rmt_read(c, (uint8_t *) rrd, 4, c->next_pkt << ED_PAGE_SHIFT);

	// subtract the size of recieve ring descriptor
	uint16_t size = rrd->count - 4;

	if (size > c->rx[i].size) {
	    printf("ne2kpci_rintr: receive buffer too small: %d > %d\n",
		   size, c->rx[i].size);
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;
	} else {
	    ne2kpci_rmt_read(c, buf, size, (c->next_pkt << ED_PAGE_SHIFT) + 4);
	    c->rx[i].nb->actual_count = size;
	    //ne2kpci_print_packet(&c->rx[i]) ;
	}
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	c->rx_head = (i + 1) % NE2KPCI_RX_SLOTS;
	if (c->rx_head == c->rx_nextq)
	    c->rx_head = -1;

	ne2kpci_flush_packet(c, rrd);
    }
}

int64_t
ne2kpci_wait(void* arg, uint64_t waiterid, int64_t waitgen)
{
    struct ne2kpci_card* c = arg;
    int64_t r = sys_udev_wait(c->obj, waiterid, waitgen);
    if (r == -E_AGAIN) {
	ne2kpci_reset(c);
	return r;
    }

    uint8_t status = inb(c->iobase + ED_P0_ISR);

    if (status & (ED_ISR_PRX | ED_ISR_OVW)) {
	outb(c->iobase + ED_P0_ISR, ED_ISR_PRX);
	outb(c->iobase + ED_P0_ISR, ED_ISR_OVW);

	if (status & ED_ISR_OVW) {
	    // how netbsd handles overwrite
	    ne2kpci_stop(c);
	    ne2kpci_reset(c);
	} else
	    ne2kpci_rintr(c);
    }
    // nic inited to reject rxp with errors
    if (status & ED_ISR_RXE)
	printf("ne2kpci_intr: packet rx error\n");

    if (status & ED_ISR_TXE)
	printf("ne2kpci_intr: packet tx error\n");

    outb(c->iobase + ED_P0_ISR, 0xFF);
    return r;
}

int
ne2kpci_init(struct cobj_ref obj, void** arg)
{
    struct ne2kpci_card *c = malloc(sizeof(*c));
    memset(c, 0, sizeof(*c));
    c->obj = obj;
    *arg = c;
    return ne2kpci_reset(c);
}
