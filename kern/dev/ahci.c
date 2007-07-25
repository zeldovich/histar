#include <dev/ahci.h>
#include <dev/disk.h>
#include <dev/pcireg.h>
#include <dev/ahcireg.h>
#include <dev/idereg.h>
#include <dev/satareg.h>
#include <kern/arch.h>
#include <kern/timer.h>
#include <inc/error.h>

struct ahci_hba {
    uint32_t irq;
    uint32_t membase;
    uint32_t ncs;			/* # control slots */
    volatile struct ahci_reg *r;
    volatile struct ahci_recv_fis *rfis[32];
    volatile struct ahci_cmd_header *cmd[32];
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

    struct ahci_cmd_table *cmd = pa2kva(a->cmd[port]->ctba);
    assert(iov_cnt < sizeof(cmd->prdt) / sizeof(cmd->prdt[0]));

    for (uint32_t slot = 0; slot < iov_cnt; slot++) {
	cmd->prdt[slot].dba = kva2pa(iov_buf[slot].iov_base);
	cmd->prdt[slot].dbc = iov_buf[slot].iov_len - 1;
	nbytes += iov_buf[slot].iov_len;
    }

    memcpy(&cmd->cfis[0], fis, fislen);
    a->cmd[port]->prdtl = iov_cnt;
    a->cmd[port]->flags = fislen / sizeof(uint32_t);

    return nbytes;
}

/*
 * Memory allocation.
 */

struct ahci_malloc_state {
    void *p;
    uint32_t off;
};

static void *
ahci_malloc(struct ahci_malloc_state *ms, uint32_t bytes)
{
    assert(bytes <= PGSIZE);

    if (!ms->p || bytes > PGSIZE - ms->off) {
	int r = page_alloc(&ms->p);
	if (r < 0)
	    return 0;
	ms->off = 0;
    }

    void *m = ms->p + ms->off;
    ms->off += bytes;

    memset(m, bytes, 0);
    return m;
}

static int
ahci_alloc(struct ahci_hba **ap)
{
    struct ahci_malloc_state ms;
    memset(&ms, 0, sizeof(ms));

    struct ahci_hba *a = ahci_malloc(&ms, sizeof(*a));
    if (!a)
	return -E_NO_MEM;

    for (int i = 0; i < 32; i++) {
	a->rfis[i] = ahci_malloc(&ms, sizeof(struct ahci_recv_fis));
	a->cmd[i] = ahci_malloc(&ms, sizeof(struct ahci_cmd_header));
	if (!a->rfis[i] || !a->cmd[i])
	    return -E_NO_MEM;

	struct ahci_cmd_table *ctab = ahci_malloc(&ms, sizeof(*ctab));
	if (!ctab)
	    return -E_NO_MEM;

	a->cmd[i]->ctba = kva2pa(ctab);
    }

    *ap = a;
    return 0;
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
    a->cmd[port]->flags |= 0;		/* _W for writes */
    a->cmd[port]->prdbc = 0;		/* len for writes */
    a->rfis[port]->reg.status = IDE_STAT_BSY;

    a->r->port[port].ci |= 1;
    cprintf("ahci_port_reset: FIS issued (%d DMA bytes)\n", len);

    uint64_t ts_start = karch_get_tsc();
    for (;;) {
	uint8_t stat = a->rfis[port]->reg.status;
	if ((stat & (IDE_STAT_BSY | IDE_STAT_DRDY)) == IDE_STAT_DRDY)
	    break;

	uint64_t ts_diff = karch_get_tsc() - ts_start;
	if (ts_diff > 1024 * 1024 * 1024) {
	    cprintf("ahci_reset_port: stuck for %"PRIu64" cycles, status %02x\n",
		    ts_diff, stat);
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
    a->ncs = AHCI_CAP_NCS(a->r->cap);

    for (uint32_t i = 0; i < 32; i++) {
	a->r->port[i].clb = kva2pa((void *) a->cmd[i]);
	a->r->port[i].fb = kva2pa((void *) a->rfis[i]);
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
    int r = ahci_alloc(&a);
    if (r < 0)
	return r;

    pci_func_enable(f);
    a->irq = f->irq_line;
    a->membase = f->reg_base[5];
    a->r = pa2kva(a->membase);

    ahci_reset(a);

    cprintf("AHCI: base 0x%x, irq %d, v 0x%x\n",
	    a->membase, a->irq, a->r->vs);
    return 1;
}
