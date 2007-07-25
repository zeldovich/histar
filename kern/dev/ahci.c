#include <kern/disk.h>
#include <dev/ahci.h>
#include <dev/pcireg.h>
#include <dev/ahcireg.h>
#include <dev/idereg.h>
#include <dev/satareg.h>
#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <inc/error.h>

struct ahci_port_page {
    volatile struct ahci_recv_fis rfis;		/* 256-byte alignment */
    uint8_t pad[0x300];

    volatile struct ahci_cmd_header cmdh;	/* 1024-byte alignment */
    struct ahci_cmd_header cmdh_unused[31];

    volatile struct ahci_cmd_table cmdt;	/* 128-byte alignment */
    struct disk dk;
};

struct ahci_hba {
    uint32_t irq;
    uint32_t membase;
    volatile struct ahci_reg *r;
    struct ahci_port_page *port[32];
};

/*
 * Helper functions
 */

static uint32_t
ahci_build_prd(struct ahci_hba *a, uint32_t port,
	       struct kiovec *iov_buf, uint32_t iov_cnt,
	       void *fis, uint32_t fislen)
{
    uint32_t nbytes = 0;

    struct ahci_cmd_table *cmd = (void *) &a->port[port]->cmdt;
    assert(iov_cnt < sizeof(cmd->prdt) / sizeof(cmd->prdt[0]));

    for (uint32_t slot = 0; slot < iov_cnt; slot++) {
	cmd->prdt[slot].dba = kva2pa(iov_buf[slot].iov_base);
	cmd->prdt[slot].dbc = iov_buf[slot].iov_len - 1;
	nbytes += iov_buf[slot].iov_len;
    }

    memcpy(&cmd->cfis[0], fis, fislen);
    a->port[port]->cmdh.prdtl = iov_cnt;
    a->port[port]->cmdh.flags = fislen / sizeof(uint32_t);

    return nbytes;
}

static void
ahci_port_debug(struct ahci_hba *a, uint32_t port)
{
    cprintf("AHCI port %d dump:\n", port);
    cprintf("PxCMD    = 0x%x\n", a->r->port[port].cmd);
    cprintf("PxTFD    = 0x%x\n", a->r->port[port].tfd);
    cprintf("PxSIG    = 0x%x\n", a->r->port[port].sig);
    cprintf("PxCI     = 0x%x\n", a->r->port[port].ci);
    cprintf("SStatus  = 0x%x\n", a->r->port[port].ssts);
    cprintf("SControl = 0x%x\n", a->r->port[port].sctl);
    cprintf("SError   = 0x%x\n", a->r->port[port].serr);
    cprintf("GHC      = 0x%x\n", a->r->ghc);
}

/*
 * Driver hooks.
 */

static void
ahci_poll(struct disk *dk)
{
}

static int
ahci_issue(struct disk *dk, disk_op op, struct kiovec *iov_buf, int iov_cnt,
	   uint64_t offset, disk_callback cb, void *cbarg)
{
    return -E_BUSY;
}

/*
 * Initialization.
 */

static void
ahci_reset_port(struct ahci_hba *a, uint32_t port)
{
    /* Wait for port to quiesce */
    if (a->r->port[port].cmd & (AHCI_PORT_CMD_ST | AHCI_PORT_CMD_CR |
				AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_FR)) {
	cprintf("AHCI: port %d active, clearing..\n", port);
	a->r->port[port].cmd &= ~(AHCI_PORT_CMD_ST | AHCI_PORT_CMD_FRE);
	timer_delay(500 * 1000 * 1000);

	if (a->r->port[port].cmd & (AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR)) {
	    cprintf("AHCI: port %d still active, giving up\n", port);
	    return;
	}
    }

    /* Initialize memory buffers */
    a->port[port]->cmdh.ctba = kva2pa((void *) &a->port[port]->cmdt);
    a->r->port[port].clb = kva2pa((void *) &a->port[port]->cmdh);
    a->r->port[port].fb = kva2pa((void *) &a->port[port]->rfis);
    a->r->port[port].ci = 0;

    /* Clear any errors first, otherwise the chip wedges */
    a->r->port[port].serr = ~0;
    a->r->port[port].serr = 0;

    /* Enable receiving frames */
    a->r->port[port].cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST |
			    AHCI_PORT_CMD_SUD | AHCI_PORT_CMD_POD |
			    AHCI_PORT_CMD_ACTIVE;

    /* Check if there's anything there */
    uint32_t phystat = a->r->port[port].ssts;
    if (!phystat) {
	cprintf("AHCI: port %d not connected\n", port);
	return;
    }

    /* Try to send an IDENTIFY */
    static union {
	struct identify_device id;
	char buf[512];
    } id_buf;

    struct kiovec id_iov =
	{ .iov_base = &id_buf.buf[0], .iov_len = sizeof(id_buf) };

    cprintf("AHCI: identifying port %d...\n", port);

    struct sata_fis_reg fis;
    memset(&fis, 0, sizeof(fis));
    fis.type = SATA_FIS_TYPE_REG_H2D;
    fis.cflag = SATA_FIS_REG_CFLAG;
    fis.command = IDE_CMD_IDENTIFY;
    fis.sector_count = 1;

    uint32_t len = ahci_build_prd(a, port, &id_iov, 1, &fis, sizeof(fis));
    a->port[port]->cmdh.flags |= 0;		/* _W for writes */
    a->port[port]->cmdh.prdbc = 0;		/* len for writes */

    a->r->port[port].ci |= 1;
    cprintf("ahci_port_reset: FIS issued (%d DMA bytes)\n", len);

    uint64_t ts_start = karch_get_tsc();
    for (;;) {
	uint32_t tfd = a->r->port[port].tfd;
	uint8_t stat = AHCI_PORT_TFD_STAT(tfd);
	if ((stat & (IDE_STAT_BSY | IDE_STAT_DRDY)) == IDE_STAT_DRDY)
	    break;

	uint64_t ts_diff = karch_get_tsc() - ts_start;
	if (ts_diff > 1024 * 1024 * 1024) {
	    cprintf("ahci_reset_port: stuck for %"PRIu64" cycles, "
		    "status %02x, error %02x\n",
		    ts_diff, AHCI_PORT_TFD_STAT(tfd), AHCI_PORT_TFD_ERR(tfd));

	    ahci_port_debug(a, port);
	    return;
	}
    }

    /* Fill in the disk object */
    struct disk *dk = &a->port[port]->dk;
    dk->dk_arg = a;
    dk->dk_id = port;
    dk->dk_issue = &ahci_issue;
    dk->dk_poll = &ahci_poll;

    uint64_t sectors = (id_buf.id.features86 & IDE_FEATURE86_LBA48) ?
			id_buf.id.lba48_sectors : id_buf.id.lba_sectors;
    dk->dk_bytes = sectors * 512;
    memcpy(&dk->dk_model[0], id_buf.id.model, sizeof(id_buf.id.model));
    memcpy(&dk->dk_serial[0], id_buf.id.serial, sizeof(id_buf.id.serial));
    memcpy(&dk->dk_firmware[0], id_buf.id.firmware, sizeof(id_buf.id.firmware));
    static_assert(sizeof(dk->dk_model) >= sizeof(id_buf.id.model));
    static_assert(sizeof(dk->dk_serial) >= sizeof(id_buf.id.serial));
    static_assert(sizeof(dk->dk_firmware) >= sizeof(id_buf.id.firmware));
    sprintf(&dk->dk_busloc[0], "ahci.%d", port);

    disk_register(dk);
}

static void
ahci_reset(struct ahci_hba *a)
{
    a->r->ghc |= AHCI_GHC_AE;

    for (uint32_t i = 0; i < 32; i++)
	if (a->r->pi & (1 << i))
	    ahci_reset_port(a, i);

    a->r->ghc |= AHCI_GHC_IE;
}

int
ahci_init(struct pci_func *f)
{
    if (PCI_INTERFACE(f->dev_class) != 0x01) {
	cprintf("ahci_init: not an AHCI controller\n");
	return 0;
    }

    struct ahci_hba *a;
    int r = page_alloc((void **) &a);
    if (r < 0)
	return r;

    static_assert(sizeof(*a) <= PGSIZE);
    memset(a, 0, sizeof(*a));

    for (int i = 0; i < 32; i++) {
	static_assert(sizeof(a->port[i]) <= PGSIZE);
	r = page_alloc((void **) &a->port[i]);
	if (r < 0)
	    return r;
    }

    pci_func_enable(f);
    a->irq = f->irq_line;
    a->membase = f->reg_base[5];
    a->r = pa2kva(a->membase);

    cprintf("AHCI: base 0x%x, irq %d, v 0x%x\n",
	    a->membase, a->irq, a->r->vs);
    ahci_reset(a);

    return 1;
}
