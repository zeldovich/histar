#include <machine/thread.h>
#include <machine/types.h>
#include <machine/pmap.h>
#include <dev/pci.h>
#include <dev/fxp.h>
#include <dev/fxpreg.h>
#include <dev/picirq.h>
#include <kern/segment.h>
#include <kern/lib.h>
#include <kern/kobj.h>
#include <inc/queue.h>
#include <inc/netdev.h>
#include <inc/error.h>

#define FXP_RX_SLOTS	16
#define FXP_TX_SLOTS	16

struct fxp_tx_slot {
    struct fxp_cb_tx tcb;
    struct fxp_tbd tbd;
    struct netbuf_hdr *nb;
    struct Segment *sg;
};

struct fxp_rx_slot {
    struct fxp_rfa rfd;
    struct fxp_rbd rbd;
    struct netbuf_hdr *nb;
    struct Segment *sg;
};

// Static allocation ensures contiguous memory.
struct fxp_card {
    uint8_t irq_line;
    uint32_t iobase;

    struct fxp_tx_slot tx[FXP_TX_SLOTS];
    struct fxp_rx_slot rx[FXP_RX_SLOTS];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer
    bool_t rx_halted;	// receiver is not running

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer
    bool_t tx_halted;	// transmitter is not running or suspended

    int64_t waitgen;
    struct Thread_tqueue waiting;
};

static struct fxp_card the_card;

void
fxp_attach(struct pci_func *pcif)
{
    struct fxp_card *c = &the_card;

    if (pcif->reg_size[1] < 64) {
	cprintf("fxp_attach: io window too small: %d @ 0x%x\n",
		pcif->reg_size[1], pcif->reg_base[1]);
	return;
    }

    c->irq_line = pcif->irq_line;
    c->iobase = pcif->reg_base[1];
    cprintf("fxp: irq %d io 0x%x\n", c->irq_line, c->iobase);
    irq_setmask_8259A(irq_mask_8259A & ~(1 << c->irq_line));

    TAILQ_INIT(&c->waiting);

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
    }

    c->rx_head = -1;
    c->rx_nextq = 0;
    c->rx_halted = 1;

    c->tx_head = -1;
    c->tx_nextq = 0;
    c->tx_halted = 1;

    // XXX
    //
    // There's a good chance we should do more initialization here,
    // but it seems to work anyway -- perhaps because pxegrub does
    // it for us.
}

static void
fxp_thread_wakeup(struct fxp_card *c)
{
    c->waitgen++;
    if (c->waitgen <= 0)
	c->waitgen = 1;

    while (!TAILQ_EMPTY(&c->waiting)) {
	struct Thread *t = TAILQ_FIRST(&c->waiting);
	thread_set_runnable(t);
	TAILQ_REMOVE(&c->waiting, t, th_waiting);
    }
}

int64_t
fxp_thread_wait(struct Thread *t, int64_t gen)
{
    struct fxp_card *c = &the_card;

    if (gen != c->waitgen)
	return c->waitgen;

    TAILQ_INSERT_HEAD(&c->waiting, t, th_waiting);
    thread_suspend(t);
    return -E_RESTART;
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
fxp_rx_start(struct fxp_card *c)
{
    if (c->rx_head == -1) {
	cprintf("fxp_rx_start: no packets\n");
	return;
    }

    if (!c->rx_halted) {
	cprintf("fxp_rx_start: not halted\n");
	return;
    }

    fxp_scb_wait(c);
    fxp_scb_cmd(c, FXP_SCB_COMMAND_RU_ABORT);

    fxp_scb_wait(c);
    outl(c->iobase + FXP_CSR_SCB_GENERAL, kva2pa(&c->rx[c->rx_head].rfd));
    fxp_scb_cmd(c, FXP_SCB_COMMAND_RU_START);
    c->rx_halted = 0;
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
    cprintf("%p(%d/%d)", nb, nb->actual_count & FXP_SIZE_MASK, nb->size);
    if ((nb->actual_count & NETHDR_COUNT_DONE))
	cprintf(" done");
    if ((nb->actual_count & NETHDR_COUNT_ERR))
	cprintf(" err");

    unsigned char *buf = (char*) (nb + 1);
    for (int i = 0; i < 25; i++)
	cprintf(" %02x", buf[i]);
    cprintf("\n");
}

static void
fxp_intr_rx(struct fxp_card *c)
{
    if (c->rx_head == -1)
	return;

    for (;;) {
	int i = c->rx_head;
	if (!(c->rx[i].rfd.rfa_status & FXP_RFA_STATUS_C))
	    break;

	c->rx[i].nb->actual_count = c->rx[i].rbd.rbd_count & FXP_SIZE_MASK;
	c->rx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	if (!(c->rx[i].rfd.rfa_status & FXP_RFA_STATUS_OK))
	    c->rx[i].nb->actual_count |= NETHDR_COUNT_ERR;

	kobject_decpin(&c->rx[i].sg->sg_ko);
	c->rx[i].sg = 0;

	c->rx_head = (i + 1) % FXP_RX_SLOTS;
	if (c->rx_head == c->rx_nextq) {
	    c->rx_head = -1;
	    break;
	}
    }
}

static void
fxp_intr_tx(struct fxp_card *c)
{
    if (c->tx_head == -1)
	return;

    for (;;) {
	int i = c->tx_head;
	if (!(c->tx[i].tcb.cb_status & FXP_CB_STATUS_C))
	    break;

	c->tx[i].nb->actual_count |= NETHDR_COUNT_DONE;
	kobject_decpin(&c->tx[i].sg->sg_ko);
	c->tx[i].sg = 0;

	c->tx_head = (i + 1) % FXP_TX_SLOTS;
	if (c->tx_head == c->tx_nextq) {
	    c->tx_head = -1;
	    break;
	}
    }
}

void
fxp_intr()
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
	fxp_rx_start(c);
    }

    fxp_thread_wakeup(c);
}

static int
fxp_add_txbuf(struct Segment *sg, struct netbuf_hdr *nb, uint16_t size)
{
    struct fxp_card *c = &the_card;
    int slot = c->tx_nextq;

    if (slot == c->tx_head)
	return -E_NO_SPACE;

    c->tx[slot].nb = nb;
    c->tx[slot].sg = sg;
    kobject_incpin(&sg->sg_ko);
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
fxp_add_rxbuf(struct Segment *sg, struct netbuf_hdr *nb, uint16_t size)
{
    struct fxp_card *c = &the_card;
    int slot = c->rx_nextq;

    if (slot == c->rx_head)
	return -E_NO_SPACE;

    c->rx[slot].nb = nb;
    c->rx[slot].sg = sg;
    kobject_incpin(&sg->sg_ko);
    c->rx[slot].rbd.rbd_buffer = kva2pa(c->rx[slot].nb + 1);
    c->rx[slot].rbd.rbd_size = (size & FXP_SIZE_MASK) | FXP_RBD_SIZE_EL;
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

int
fxp_add_buf(struct Segment *sg, uint64_t npage, uint32_t pageoff, netbuf_type type)
{
    void *p;
    int r = kobject_get_page(&sg->sg_ko, npage, &p);
    if (r < 0)
	return r;

    if (pageoff > PGSIZE || pageoff + sizeof(struct netbuf_hdr) > PGSIZE)
	return -E_INVAL;

    struct netbuf_hdr *nb = p + pageoff;
    uint16_t size = nb->size;
    if (pageoff + sizeof(struct netbuf_hdr) + size > PGSIZE)
	return -E_INVAL;

    if (type == netbuf_rx) {
	return fxp_add_rxbuf(sg, nb, size);
    } else if (type == netbuf_tx) {
	return fxp_add_txbuf(sg, nb, size);
    } else {
	return -E_INVAL;
    }
}
