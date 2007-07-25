#include <dev/ahci.h>
#include <dev/disk.h>
#include <dev/pcireg.h>
#include <dev/ahcireg.h>
#include <dev/idereg.h>
#include <dev/satareg.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <inc/error.h>

struct ahci_port_page {
    struct ahci_recv_fis rfis;		/* 256-byte alignment */
    uint8_t pad[0x300];

    struct ahci_cmd_header cmdh;	/* 1024-byte alignment */
    struct ahci_cmd_header cmdh_unused[31];

    struct ahci_cmd_table cmdt;		/* 128-byte alignment */
};

struct ahci_hba {
    uint32_t irq;
    uint32_t membase;
    volatile struct ahci_reg *r;
    volatile struct ahci_port_page *port[32];
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

/*
 * Initialization.
 */

static void
ahci_reset_port(struct ahci_hba *a, uint32_t port)
{
    cprintf("ahci_reset_port: %d\n", port);
    a->r->port[port].ci = 0;
    a->r->port[port].cmd |= AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_ST;
    
    static union {
	struct identify_device id;
	char buf[512];
    } id_buf;

    struct kiovec id_iov =
	{ .iov_base = &id_buf.buf[0], .iov_len = sizeof(id_buf) };

    struct sata_fis_reg fis;
    memset(&fis, 0, sizeof(fis));
    fis.type = SATA_FIS_TYPE_REG_H2D;
    fis.command = IDE_CMD_IDENTIFY;

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

	    cprintf("SStatus = 0x%x\n", a->r->port[port].ssts);
	    cprintf("SControl = 0x%x\n", a->r->port[port].sctl);
	    cprintf("SError = 0x%x\n", a->r->port[port].serr);
	    return;
	}
    }

    cprintf("ahci_port_reset: FIS complete\n");
    cprintf("ahci_port_reset: model %s\n", id_buf.id.model);
}

static void
ahci_reset(struct ahci_hba *a)
{
    a->r->ghc |= AHCI_GHC_AE;
    a->r->ghc |= AHCI_GHC_HR;
    while (a->r->ghc & AHCI_GHC_HR)
	timer_delay(1000);

    a->r->ghc |= AHCI_GHC_AE;

    for (uint32_t i = 0; i < 32; i++) {
	a->port[i]->cmdh.ctba = kva2pa((void *) &a->port[i]->cmdt);
	a->r->port[i].clb = kva2pa((void *) &a->port[i]->cmdh);
	a->r->port[i].fb = kva2pa((void *) &a->port[i]->rfis);
	if (a->r->pi & (1 << i))
	    ahci_reset_port(a, i);
    }

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

    ahci_reset(a);

    cprintf("AHCI: base 0x%x, irq %d, v 0x%x\n",
	    a->membase, a->irq, a->r->vs);
    return 1;
}
