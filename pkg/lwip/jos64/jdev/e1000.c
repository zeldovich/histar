#include <machine/types.h>
#include <machine/pmap.h>
#include <machine/x86.h>
#include <machine/io.h>
#include <dev/pci.h>
#include <dev/e1000reg.h>
#include <inc/error.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/syscall.h>
#include <inc/array.h>

#include <jdev/e1000.h>
#include <jdev/jnic.h>

#include <malloc.h>
#include <string.h>

#include <unistd.h>

#define E1000_RX_SLOTS	65
#define E1000_TX_SLOTS	65

// The mapping only needs to be big enough to cover the MMIO registers
#define E1000_MMIO_PAGES 16

struct e1000_buffer_slot {
    struct netbuf_hdr *nb;
};

// Static allocation ensures contiguous memory.
struct e1000_tx_descs {
    struct wiseman_txdesc txd[E1000_TX_SLOTS] __attribute__((aligned (16)));
};

struct e1000_rx_descs {
    struct wiseman_rxdesc rxd[E1000_RX_SLOTS] __attribute__((aligned (16)));
};

struct e1000_card {
    struct cobj_ref obj;
    struct cobj_ref as;
    struct cobj_ref desc_seg;

    void *vabase;
    uint8_t mac_addr[6];

    uint32_t iobase;
    uint8_t irq_line;
    uint16_t pci_dev_id;

    struct e1000_tx_descs *txds;
    struct e1000_rx_descs *rxds;

    struct e1000_buffer_slot tx[E1000_TX_SLOTS];
    struct e1000_buffer_slot rx[E1000_RX_SLOTS];

    int rx_head;	// card receiving into rx_head, -1 if none
    int rx_nextq;	// next slot for rx buffer

    int tx_head;	// card transmitting from tx_head, -1 if none
    int tx_nextq;	// next slot for tx buffer

    void*		seg_va;
    struct cobj_ref	seg;
    uint64_t		seg_bytes;
};

#define echeck(expr)								\
    do {									\
        int64_t __r = (expr);							\
        if (__r < 0)								\
            panic("%s:%u: %s - %s", __FILE__, __LINE__, #expr, e2s(__r));	\
    } while (0)

static void
timer_delay(uint64_t nsec)
{
    uint64_t start = sys_clock_nsec();
    uint64_t end = start + nsec;
    do {
	sys_sync_wait(&start, start, end);
    } while (end > (uint64_t)sys_clock_nsec());
}

static int
e1000_as_map(struct e1000_card *c, struct cobj_ref seg, void *va, uint64_t bytes)
{
    int r;
    uint64_t i;
    struct u_address_space uas;
    struct u_segment_mapping usm[16];

    memset(&usm, 0, sizeof(usm));
    memset(&uas, 0, sizeof(uas));
    
    uas.size = array_size(usm);
    uas.ents = usm;
    
    if ((r = sys_as_get(c->as, &uas)) < 0)
	return r;
    
    if (array_size(usm) <= uas.nent)
	return -E_NO_SPACE;

    i = uas.nent;
    uas.ents[i].segment = seg;
    uas.ents[i].start_page = 0;
    uas.ents[i].num_pages = ROUNDUP(bytes, PGSIZE) / PGSIZE;
    uas.ents[i].flags = SEGMAP_READ | SEGMAP_WRITE;
    uas.ents[i].va = va;
    uas.nent = i + 1;
    
    return sys_as_set(c->as, &uas);
}

static physaddr_t
e1000_pa(struct e1000_card *c, void *va)
{
    physaddr_t pa;
    int r = sys_as_pa(c->as, va, &pa);
    if (r < 0)
	panic("sys_as_pa failed: %s\n", e2s(r));
    return pa;
}

static uint32_t
e1000_io_read(struct e1000_card *c, uint32_t reg)
{
    assert(reg + 4 <= E1000_MMIO_PAGES * PGSIZE);
    volatile uint32_t *ptr = c->vabase + reg;
    return *ptr;
}

static void
e1000_io_write(struct e1000_card *c, uint32_t reg, uint32_t val)
{
    assert(reg + 4 <= E1000_MMIO_PAGES * PGSIZE);
    volatile uint32_t *ptr = c->vabase + reg;
    *ptr = val;
}

static void
e1000_io_write_flush(struct e1000_card *c, uint32_t reg, uint32_t val)
{
    e1000_io_write(c, reg, val);
    e1000_io_read(c, WMREG_STATUS);
}

static void
e1000_eeprom_uwire_out(struct e1000_card *c, uint16_t data, uint16_t count)
{
    uint32_t mask = 1 << (count - 1);
    uint32_t eecd = e1000_io_read(c, WMREG_EECD) & ~(EECD_DO | EECD_SK);

    do {
	if (data & mask)
	    eecd |= EECD_DI;
	else
	    eecd &= ~(EECD_DI);

	e1000_io_write_flush(c, WMREG_EECD, eecd);
	timer_delay(50000);

	e1000_io_write_flush(c, WMREG_EECD, eecd | EECD_SK);
	timer_delay(50000);

	e1000_io_write_flush(c, WMREG_EECD, eecd);
	timer_delay(50000);

	mask = mask >> 1;
    } while (mask);

    e1000_io_write_flush(c, WMREG_EECD, eecd & ~(EECD_DI));
}

static uint16_t
e1000_eeprom_uwire_in(struct e1000_card *c, uint16_t count)
{
    uint32_t data = 0;
    uint32_t eecd = e1000_io_read(c, WMREG_EECD) & ~(EECD_DO | EECD_DI);

    for (uint16_t i = 0; i < count; i++) {
	data = data << 1;

	e1000_io_write_flush(c, WMREG_EECD, eecd | EECD_SK);
	timer_delay(50000);

	eecd = e1000_io_read(c, WMREG_EECD) & ~(EECD_DI);
	if (eecd & EECD_DO)
	    data |= 1;

	e1000_io_write_flush(c, WMREG_EECD, eecd & ~EECD_SK);
	timer_delay(50000);
    }

    return data;
}

static int32_t
e1000_eeprom_uwire_read(struct e1000_card *c, uint16_t off)
{
    /* Make sure this is microwire */
    uint32_t eecd = e1000_io_read(c, WMREG_EECD);
    if (eecd & EECD_EE_TYPE) {
	cprintf("e1000_eeprom_read: EERD timeout, SPI not supported\n");
	return -1;
    }

    uint32_t abits = (eecd & EECD_EE_SIZE) ? 8 : 6;

    /* Get access to the EEPROM */
    eecd |= EECD_EE_REQ;
    e1000_io_write_flush(c, WMREG_EECD, eecd);
    for (uint32_t t = 0; t < 100; t++) {
	timer_delay(50000);
	eecd = e1000_io_read(c, WMREG_EECD);
	if (eecd & EECD_EE_GNT)
	    break;
    }

    if (!(eecd & EECD_EE_GNT)) {
	cprintf("e1000_eeprom_read: cannot get EEPROM access\n");
	e1000_io_write_flush(c, WMREG_EECD, eecd & ~EECD_EE_REQ);
	return -1;
    }

    /* Turn on the EEPROM */
    eecd &= ~(EECD_DI | EECD_SK);
    e1000_io_write_flush(c, WMREG_EECD, eecd);

    eecd |= EECD_CS;
    e1000_io_write_flush(c, WMREG_EECD, eecd);

    /* Read the bits */
    e1000_eeprom_uwire_out(c, UWIRE_OPC_READ, 3);
    e1000_eeprom_uwire_out(c, off, abits);
    uint16_t v = e1000_eeprom_uwire_in(c, 16);

    /* Turn off the EEPROM */
    eecd &= ~(EECD_CS | EECD_DI | EECD_SK);
    e1000_io_write_flush(c, WMREG_EECD, eecd);

    e1000_io_write_flush(c, WMREG_EECD, eecd | EECD_SK);
    timer_delay(50000);

    e1000_io_write_flush(c, WMREG_EECD, eecd & ~EECD_EE_REQ);
    timer_delay(50000);

    return v;
}

static int32_t
e1000_eeprom_eerd_read(struct e1000_card *c, uint16_t off)
{
    e1000_io_write(c, WMREG_EERD, (off << EERD_ADDR_SHIFT) | EERD_START);

    uint32_t reg;
    for (int x = 0; x < 100; x++) {
	reg = e1000_io_read(c, WMREG_EERD);
	if (!(reg & EERD_DONE))
	    timer_delay(5000);
    }

    if (reg & EERD_DONE)
	return (reg & EERD_DATA_MASK) >> EERD_DATA_SHIFT;
    return -1;
}

static int
e1000_eeprom_read(struct e1000_card *c, uint16_t *buf, int off, int count)
{
    for (int i = 0; i < count; i++) {
	int32_t r = e1000_eeprom_eerd_read(c, off + i);
	if (r < 0)
	    r = e1000_eeprom_uwire_read(c, off + i);

	if (r < 0) {
	    cprintf("e1000_eeprom_read: cannot read\n");
	    return -1;
	}

	buf[i] = r;
    }

    return 0;
}

static int
e1000_reset(struct e1000_card *c)
{
    int r;
    uint64_t bytes;

    if (c->vabase)
	if ((r = segment_unmap_delayed(c->vabase, 1)) < 0)
	    panic("segment_unmap_delayed failed: %s\n", e2s(r));

    bytes = PGSIZE * E1000_MMIO_PAGES;
    c->vabase = 0;
    r = segment_map(c->obj, 0, SEGMAP_READ | SEGMAP_WRITE, &c->vabase,
		    &bytes, 0);
    if (r < 0)
	return r;

    // Get the MAC address
    uint16_t myaddr[3];
    r = e1000_eeprom_read(c, &myaddr[0], EEPROM_OFF_MACADDR, 3);
    if (r < 0) {
	printf("e1000_reset: cannot read EEPROM MAC addr: %s\n", e2s(r));
	return r;
    }

    for (int i = 0; i < 3; i++) {
	c->mac_addr[2*i + 0] = myaddr[i] & 0xff;
	c->mac_addr[2*i + 1] = myaddr[i] >> 8;
    }

    e1000_io_write(c, WMREG_RCTL, 0);
    e1000_io_write(c, WMREG_TCTL, 0);

    // Allocate the card's packet buffer memory equally between rx, tx
    uint32_t pba = e1000_io_read(c, WMREG_PBA);
    uint32_t rxtx = ((pba >> PBA_RX_SHIFT) & PBA_RX_MASK) +
		    ((pba >> PBA_TX_SHIFT) & PBA_TX_MASK);
    e1000_io_write(c, WMREG_PBA, rxtx / 2);

    // Reset PHY, card
    uint32_t ctrl = e1000_io_read(c, WMREG_CTRL);
    e1000_io_write(c, WMREG_CTRL, ctrl | CTRL_PHY_RESET);
    timer_delay(5 * 1000 * 1000);

    e1000_io_write(c, WMREG_CTRL, ctrl | CTRL_RST);
    timer_delay(10 * 1000 * 1000);

    for (int i = 0; i < 1000; i++) {
	if ((e1000_io_read(c, WMREG_CTRL) & CTRL_RST) == 0)
	    break;
	timer_delay(20000);
    }

    if (e1000_io_read(c, WMREG_CTRL) & CTRL_RST)
	cprintf("e1000_reset: card still resetting, odd..\n");

    e1000_io_write(c, WMREG_CTRL, ctrl | CTRL_SLU | CTRL_ASDE);

    // Make sure the management hardware is not hiding any packets
    if (c->pci_dev_id == 0x108c || c->pci_dev_id == 0x109a) {
	uint32_t manc = e1000_io_read(c, WMREG_MANC);
	manc &= ~MANC_ARP_REQ;
	manc |= MANC_MNG2HOST;

	e1000_io_write(c, WMREG_MANC2H, MANC2H_PORT_623 | MANC2H_PORT_664);
	e1000_io_write(c, WMREG_MANC, manc);
    }

    // Setup RX, TX rings
    uint64_t rptr = e1000_pa(c, &c->rxds->rxd[0]);
    e1000_io_write(c, WMREG_RDBAH, rptr >> 32);
    e1000_io_write(c, WMREG_RDBAL, rptr & 0xffffffff);
    e1000_io_write(c, WMREG_RDLEN, sizeof(c->rxds->rxd));
    e1000_io_write(c, WMREG_RDH, 0);
    e1000_io_write(c, WMREG_RDT, 0);
    e1000_io_write(c, WMREG_RDTR, 0);
    e1000_io_write(c, WMREG_RADV, 0);

    uint64_t tptr = e1000_pa(c, &c->txds->txd[0]);
    e1000_io_write(c, WMREG_TDBAH, tptr >> 32);
    e1000_io_write(c, WMREG_TDBAL, tptr & 0xffffffff);
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
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 0,
		   (c->mac_addr[0]) |
		   (c->mac_addr[1] << 8) |
		   (c->mac_addr[2] << 16) |
		   (c->mac_addr[3] << 24));
    e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + 4,
		   (c->mac_addr[4]) |
		   (c->mac_addr[5] << 8) | RAL_AV);

    for (int i = 2; i < WM_RAL_TABSIZE * 2; i++)
	e1000_io_write(c, WMREG_CORDOVA_RAL_BASE + i * 4, 0);
    for (int i = 0; i < WM_MC_TABSIZE; i++)
	e1000_io_write(c, WMREG_CORDOVA_MTA + i * 4, 0);

    // Enable RX, TX
    e1000_io_write(c, WMREG_RCTL,
		   RCTL_EN | RCTL_RDMTS_1_2 | RCTL_DPF | RCTL_BAM);
    e1000_io_write(c, WMREG_TCTL,
		   TCTL_EN | TCTL_PSP | TCTL_CT(TX_COLLISION_THRESHOLD) |
		   TCTL_COLD(TX_COLLISION_DISTANCE_FDX));

    c->rx_head = -1;
    c->rx_nextq = 0;

    c->tx_head = -1;
    c->tx_nextq = 0;

    return 0;
}

static void
e1000_intr_rx(struct e1000_card *c)
{
    for (;;) {
	int i = c->rx_head;
	if (i == -1 || !(c->rxds->rxd[i].wrx_status & WRX_ST_DD))
	    break;

	//kobject_unpin_page(&c->rx[i].sg->sg_ko);
	//pagetree_decpin_write(c->rx[i].nb);
	//kobject_dirty(&c->rx[i].sg->sg_ko);
	//c->rx[i].sg = 0;
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

	//kobject_unpin_page(&c->tx[i].sg->sg_ko);
	//pagetree_decpin_write(c->tx[i].nb);
	//kobject_dirty(&c->tx[i].sg->sg_ko);
	//c->tx[i].sg = 0;
	c->tx[i].nb->actual_count |= NETHDR_COUNT_DONE;

	c->tx_head = (i + 1) % E1000_TX_SLOTS;
	if (c->tx_head == c->tx_nextq)
	    c->tx_head = -1;
    }
}

static int64_t
e1000_wait(void *arg, uint64_t waiterid, int64_t waitgen)
{
    struct e1000_card* c = arg;
    int64_t r = sys_udev_wait(c->obj, waiterid, waitgen);
    if (r == -E_AGAIN) {
	e1000_reset(c);
	return r;
    } else if (r < 0) {
	return r;
    }

    assert(sys_self_umask_enable(c->obj) == 0);

    uint32_t icr = e1000_io_read(c, WMREG_ICR);

    if (icr & ICR_TXDW)
	e1000_intr_tx(c);

    if (icr & ICR_RXT0)
	e1000_intr_rx(c);

    if (icr & ICR_RXO) {
	cprintf("e1000_intr: receiver overrun\n");
	e1000_reset(c);
    }

    assert(sys_self_umask_disable() == 0);
    return r;
}

static int
e1000_add_txbuf(void *arg, struct netbuf_hdr *nb, uint16_t size)
{
    struct e1000_card *c = arg;
    int slot = c->tx_nextq;
    int next_slot = (slot + 1) % E1000_TX_SLOTS;

    if (slot == c->tx_head || next_slot == c->tx_head)
	return -E_NO_SPACE;

    if (size > 1522) {
	printf("e1000_add_txbuf: oversize buffer, %d bytes\n", size);
	return -E_INVAL;
    }

    c->tx[slot].nb = nb;

    c->txds->txd[slot].wtx_addr = e1000_pa(c, c->tx[slot].nb + 1);
    c->txds->txd[slot].wtx_cmdlen = size | WTX_CMD_RS | WTX_CMD_EOP | WTX_CMD_IFCS;
    memset(&c->txds->txd[slot].wtx_fields, 0, sizeof(&c->txds->txd[slot].wtx_fields));

    c->tx_nextq = next_slot;
    if (c->tx_head == -1)
	c->tx_head = slot;

    e1000_io_write(c, WMREG_TDT, next_slot);
    return 0;
}

static int
e1000_add_rxbuf(void *arg, struct netbuf_hdr *nb, uint16_t size)
{
    struct e1000_card *c = arg;
    int slot = c->rx_nextq;
    int next_slot = (slot + 1) % E1000_RX_SLOTS;

    if (slot == c->rx_head || next_slot == c->rx_head)
	return -E_NO_SPACE;

    // The receive buffer size is hard-coded in the RCTL register as 2K.
    // However, we configure it to reject packets over 1522 bytes long.
    if (size < 1522) {
	printf("e1000_add_rxbuf: buffer too small, %d bytes\n", size);
	return -E_INVAL;
    }

    c->rx[slot].nb = nb;

    memset(&c->rxds->rxd[slot], 0, sizeof(c->rxds->rxd[slot]));
    c->rxds->rxd[slot].wrx_addr = e1000_pa(c, c->rx[slot].nb + 1);

    c->rx_nextq = next_slot;
    if (c->rx_head == -1)
	c->rx_head = slot;

    e1000_io_write(c, WMREG_RDT, next_slot);
    return 0;
}

static int
e1000_buf(void *arg, struct cobj_ref seg,
	  uint64_t offset, netbuf_type type)
{
    int r;
    struct e1000_card* c = arg;
    // XXX client can only use one segment
    if (!c->seg_va) {
	c->seg = seg;
	r = segment_map(c->seg, 0, SEGMAP_READ | SEGMAP_WRITE, 
			&c->seg_va, &c->seg_bytes, 0);
	if (r < 0)
	    return r;
	r = e1000_as_map(c, c->seg, c->seg_va, c->seg_bytes);
	if (r < 0)
	    return r;
    }

    struct netbuf_hdr* nb = c->seg_va + offset;
    uint16_t size = nb->size;
    
    switch (type) {
    case netbuf_rx:
	return e1000_add_rxbuf(c, nb, size);
    case netbuf_tx:
	return e1000_add_txbuf(c, nb, size);
    default:
	return -E_INVAL;
    }
}

static int 
e1000_macaddr(void *arg, uint8_t* macaddr)
{
    struct e1000_card *c = arg;
    memcpy(macaddr, c->mac_addr, 6);
    return 0;
}

static int
e1000_init(struct cobj_ref obj, void** arg)
{
    int64_t r;
    uint64_t bytes, ct;
    void *va = 0;
    struct e1000_card *c = 0;

    c = malloc(sizeof(*c));
    if (!c)
	panic("no mem");
    memset(c, 0, sizeof(*c));
    c->obj = obj;

    ct = start_env->shared_container;

    echeck(r = sys_as_create(ct, 0, "e1000-as"));
    c->as = COBJ(ct, r);
    echeck(sys_device_set_as(obj, c->as));

    bytes = sizeof(*c->txds) + sizeof(*c->rxds);
    echeck(segment_alloc(ct, bytes, &c->desc_seg, &va, 0, "e1000-desc"));
    echeck(e1000_as_map(c, c->desc_seg, va, bytes));
    c->txds = va;
    c->rxds = va + sizeof(*c->txds);

    *arg = c;
  
    return e1000_reset(c);
}

struct jnic_device e1000_jnic = {
    .init	   = e1000_init,
    .net_macaddr   = e1000_macaddr,
    .net_buf	   = e1000_buf,
    .net_wait	   = e1000_wait,
};
