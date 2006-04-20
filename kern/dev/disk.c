#include <machine/x86.h>
#include <machine/pmap.h>
#include <dev/ide.h>
#include <dev/disk.h>
#include <kern/lib.h>
#include <kern/intr.h>
#include <inc/error.h>

struct ide_op {
    disk_op op;
    struct iovec *iov_buf;
    int iov_cnt;
    uint64_t byte_offset;
    disk_callback cb;
    void *cbarg;
};

// This is really an IDE channel coupled with one drive
struct ide_channel {
    // Hardware interface
    uint32_t cmd_addr;
    uint32_t ctl_addr;
    uint32_t bm_addr;
    uint32_t irq;
    struct interrupt_handler ih;

    // Flags
    bool_t dma_wait;
    bool_t irq_wait;

    // Status values
    uint8_t ide_status;
    uint8_t ide_error;
    uint8_t dma_status;

    // 1-deep command queue
    struct ide_op current_op;

    // Align to 256 bytes to avoid spanning a 64K boundary.
    // 17 slots is enough for up to 64K data (16 pages), the max for DMA.
#define NPRDSLOTS	17
    struct ide_prd bm_prd[NPRDSLOTS] __attribute__((aligned (256)));
};

// One global IDE channel and drive on it, for now
static struct ide_channel the_ide_channel;
static uint32_t the_ide_drive;

static int
ide_wait(struct ide_channel *idec, uint8_t flagmask, uint8_t flagset)
{
    uint64_t ts_start = read_tsc();
    for (;;) {
	idec->ide_status = inb(idec->cmd_addr + IDE_REG_STATUS);
	if ((idec->ide_status & (IDE_STAT_BSY | flagmask)) == flagset)
	    break;

	uint64_t ts_diff = read_tsc() - ts_start;
	if (ts_diff > 1024 * 1024 * 1024) {
	    cprintf("ide_wait: stuck for %ld cycles, status %02x\n",
		    ts_diff, idec->ide_status);
	    return -E_BUSY;
	}
    }

    if (idec->ide_status & IDE_STAT_ERR) {
	idec->ide_error = inb(idec->cmd_addr + IDE_REG_ERROR);
	cprintf("ide_wait: error, status %02x error bits %02x\n",
		idec->ide_status, idec->ide_error);
    }

    if (idec->ide_status & IDE_STAT_DF)
	cprintf("ide_wait: data error, status %02x\n", idec->ide_status);

    return 0;
}

static void
ide_select_drive(struct ide_channel *idec, uint32_t diskno)
{
    outb(idec->cmd_addr + IDE_REG_DEVICE, (diskno << 4));
}

static void
ide_select_sectors(struct ide_channel *idec, uint32_t diskno,
		   uint32_t start_sector, uint32_t num_sectors)
{
    assert(num_sectors <= 256);

    // 28-bit addressing mode
    outb(idec->cmd_addr + IDE_REG_SECTOR_COUNT, num_sectors & 0xff);
    outb(idec->cmd_addr + IDE_REG_LBA_LOW, start_sector & 0xff);
    outb(idec->cmd_addr + IDE_REG_LBA_MID, (start_sector >> 8) & 0xff);
    outb(idec->cmd_addr + IDE_REG_LBA_HI, (start_sector >> 16) & 0xff);
    outb(idec->cmd_addr + IDE_REG_DEVICE, IDE_DEV_LBA |
					  (diskno << 4) |
					  ((start_sector >> 24) & 0x0f));
}

static int
ide_pio_in(struct ide_channel *idec, void *buf, uint32_t num_sectors)
{
    char *cbuf = (char *) buf;

    for (; num_sectors > 0; num_sectors--, cbuf += 512) {
	int r = ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
	if (r < 0)
	    return r;

	if ((idec->ide_status & (IDE_STAT_DF | IDE_STAT_ERR)))
	    return -E_IO;

	insl(idec->cmd_addr + IDE_REG_DATA, cbuf, 512 / 4);
    }

    return 0;
}

static int __attribute__((__unused__))
ide_pio_out(struct ide_channel *idec, const void *buf, uint32_t num_sectors)
{
    const char *cbuf = (const char *) buf;

    for (; num_sectors > 0; num_sectors--, cbuf += 512) {
	int r = ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
	if (r < 0)
	    return r;

	if ((idec->ide_status & (IDE_STAT_DF | IDE_STAT_ERR)))
	    return -E_IO;

	outsl(idec->cmd_addr + IDE_REG_DATA, cbuf, 512 / 4);
    }

    return 0;
}

static void
ide_complete(struct ide_channel *idec, disk_io_status stat)
{
    if (SAFE_EQUAL(idec->current_op.op, op_none))
	return;

    if (SAFE_EQUAL(stat, disk_io_failure))
	cprintf("ide_complete: %s error at byte offset %lu\n",
		SAFE_EQUAL(idec->current_op.op, op_read) ? "read" :
		SAFE_EQUAL(idec->current_op.op, op_write) ? "write" :
		SAFE_EQUAL(idec->current_op.op, op_flush) ? "flush" : "unknown",
		idec->current_op.byte_offset);

    idec->current_op.op = op_none;
    idec->current_op.cb(stat, idec->current_op.cbarg);
}

static uint32_t
ide_dma_init(struct ide_channel *idec, disk_op op,
	     struct iovec *iov_buf, int iov_cnt)
{
    int prd_slot = 0;
    int iov_slot = 0;
    uint32_t iov_slot_start = 0;
    uint32_t nbytes = 0;

    while (iov_slot < iov_cnt) {
	assert(prd_slot < NPRDSLOTS);

	void *buf = iov_buf[iov_slot].iov_base + iov_slot_start;
	uint32_t bytes = iov_buf[iov_slot].iov_len - iov_slot_start;

	int page_off = PGOFF(buf);
	uint32_t page_bytes = PGSIZE - page_off;
	if (page_bytes > bytes)
	    page_bytes = bytes;

	idec->bm_prd[prd_slot].addr = kva2pa(buf);
	idec->bm_prd[prd_slot].count = page_bytes & 0xffff;

	iov_slot_start += page_bytes;
	nbytes += page_bytes;

	if (iov_slot_start == iov_buf[iov_slot].iov_len) {
	    iov_slot++;
	    iov_slot_start = 0;
	}

	if (iov_slot == iov_cnt)
	    idec->bm_prd[prd_slot].count |= IDE_PRD_EOT;

	prd_slot++;
    }

    outl(idec->bm_addr + IDE_BM_PRDT_REG, kva2pa(&idec->bm_prd[0]));
    outb(idec->bm_addr + IDE_BM_STAT_REG,
	 IDE_BM_STAT_D0_DMA | IDE_BM_STAT_D1_DMA |
	 IDE_BM_STAT_INTR | IDE_BM_STAT_ERROR);
    outb(idec->bm_addr + IDE_BM_CMD_REG,
	 SAFE_EQUAL(op, op_read) ? IDE_BM_CMD_WRITE : 0);

    return nbytes;
}

static void
ide_dma_start(struct ide_channel *idec)
{
    outb(idec->bm_addr + IDE_BM_CMD_REG,
	 IDE_BM_CMD_START | inb(idec->bm_addr + IDE_BM_CMD_REG));
}

static int
ide_dma_finish(struct ide_channel *idec)
{
    idec->dma_status = inb(idec->bm_addr + IDE_BM_STAT_REG);
    if (!(idec->dma_status & IDE_BM_STAT_INTR))
	return -E_IO;

    outb(idec->bm_addr + IDE_BM_CMD_REG, 0);
    return 0;
}

static void
ide_dma_irqack(struct ide_channel *idec)
{
    outb(idec->bm_addr + IDE_BM_STAT_REG,
	 inb(idec->bm_addr + IDE_BM_STAT_REG));
}

void
ide_intr(void)
{
    int r;
    struct ide_channel *idec = &the_ide_channel;

    if (!idec->irq_wait) {
	inb(idec->cmd_addr + IDE_REG_STATUS);
	return;
    }

    if (idec->dma_wait) {
	r = ide_dma_finish(idec);
	if (r < 0)
	    return;

	idec->dma_wait = 0;
    }

    r = ide_wait(idec, 0, 0);
    if (r < 0) {
	cprintf("ide_intr: timed out waiting for unbusy\n");
	ide_complete(idec, disk_io_failure);
	return;
    }

    ide_dma_irqack(idec);

    r = ide_wait(idec, IDE_STAT_DRDY | IDE_STAT_DRQ, IDE_STAT_DRDY);
    if (r < 0) {
	cprintf("ide_intr: timed out waiting for DRDY\n");
	ide_complete(idec, disk_io_failure);
	return;
    }

    if ((idec->ide_status & (IDE_STAT_BSY | IDE_STAT_DF |
			     IDE_STAT_ERR | IDE_STAT_DRQ)) ||
	(idec->dma_status & (IDE_BM_STAT_ERROR | IDE_BM_STAT_ACTIVE)))
    {
	cprintf("ide_intr: IDE error %02x error bits %02x DMA status %02x\n",
		idec->ide_status, idec->ide_error, idec->dma_status);
	ide_complete(idec, disk_io_failure);
	return;
    }

    ide_complete(idec, disk_io_success);
}

static int
ide_send(struct ide_channel *idec, uint32_t diskno)
{
    ide_select_drive(idec, diskno);
    int r = ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
    if (r < 0)
	return r;

    idec->irq_wait = 0;
    idec->dma_wait = 0;

    if (SAFE_EQUAL(idec->current_op.op, op_read) ||
	SAFE_EQUAL(idec->current_op.op, op_write))
    {
	uint32_t num_bytes = ide_dma_init(idec, idec->current_op.op,
						idec->current_op.iov_buf,
						idec->current_op.iov_cnt);

	if (num_bytes > (1 << 16))
	    panic("ide_send: request too big for IDE DMA: %d", num_bytes);

	assert((idec->current_op.byte_offset % 512) == 0);
	assert((num_bytes % 512) == 0);

	ide_select_sectors(idec, diskno, idec->current_op.byte_offset / 512,
					 num_bytes / 512);
	outb(idec->cmd_addr + IDE_REG_CMD,
	    SAFE_EQUAL(idec->current_op.op, op_read) ? IDE_CMD_READ_DMA
						     : IDE_CMD_WRITE_DMA);
	ide_dma_start(idec);
	idec->dma_wait = 1;
    } else if (SAFE_EQUAL(idec->current_op.op, op_flush)) {
	outb(idec->cmd_addr + IDE_REG_CMD, IDE_CMD_FLUSH_CACHE);
    } else {
	panic("ide_send: unknown op %d", SAFE_UNWRAP(idec->current_op.op));
    }

    idec->irq_wait = 1;
    idec->ide_error = 0;

    return 0;
}

static void
ide_string_shuffle(char *s, int len)
{
    for (int i = 0; i < len; i += 2) {
	char c = s[i+1];
	s[i+1] = s[i];
	s[i] = c;
    }
}

static union {
    struct identify_device id;
    char buf[512];
} identify_buf;

static void
ide_init(struct ide_channel *idec, uint32_t diskno)
{
    outb(idec->cmd_addr + IDE_REG_DEVICE, diskno << 4);
    ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);

    outb(idec->cmd_addr + IDE_REG_DEVICE, diskno << 4);
    outb(idec->cmd_addr + IDE_REG_CMD, IDE_CMD_IDENTIFY);

    cprintf("Trying to identify IDE disk\n");
    if (ide_pio_in(idec, &identify_buf, 1) < 0) {
	cprintf("Unable to identify disk device\n");
	return;
    }

    ide_string_shuffle(identify_buf.id.serial,
		       sizeof(identify_buf.id.serial));
    ide_string_shuffle(identify_buf.id.model,
		       sizeof(identify_buf.id.model));
    ide_string_shuffle(identify_buf.id.firmware,
		       sizeof(identify_buf.id.firmware));

    int udma_mode = -1;
    for (int i = 0; i < 5; i++)
	if ((identify_buf.id.udma_mode & (1 << i)))
	    udma_mode = i;

    cprintf("IDE device (%d sectors, UDMA %d%s): %1.40s\n",
	    identify_buf.id.lba_sectors, udma_mode,
	    idec->bm_addr ? ", bus-master" : "",
	    identify_buf.id.model);

    if (!(identify_buf.id.hwreset & IDE_HWRESET_CBLID)) {
	cprintf("IDE: 80-pin cable absent, not enabling UDMA\n");
	udma_mode = -1;
    }

    if (udma_mode >= 0) {
	outb(idec->cmd_addr + IDE_REG_DEVICE, diskno << 4);
	outb(idec->cmd_addr + IDE_REG_FEATURES, IDE_FEATURE_XFER_MODE);
	outb(idec->cmd_addr + IDE_REG_SECTOR_COUNT, IDE_XFER_MODE_UDMA | udma_mode);
	outb(idec->cmd_addr + IDE_REG_CMD, IDE_CMD_SETFEATURES);

	ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
	if ((idec->ide_status & (IDE_STAT_DF | IDE_STAT_ERR)))
	    cprintf("IDE: Unable to enable UDMA\n");
    }

    // Enable write-caching
    outb(idec->cmd_addr + IDE_REG_DEVICE, diskno << 4);
    outb(idec->cmd_addr + IDE_REG_FEATURES, IDE_FEATURE_WCACHE_ENA);
    outb(idec->cmd_addr + IDE_REG_CMD, IDE_CMD_SETFEATURES);

    ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
    if ((idec->ide_status & (IDE_STAT_DF | IDE_STAT_ERR)))
	cprintf("IDE: Unable to enable write-caching\n");

    // Enable read look-ahead
    outb(idec->cmd_addr + IDE_REG_DEVICE, diskno << 4);
    outb(idec->cmd_addr + IDE_REG_FEATURES, IDE_FEATURE_RLA_ENA);
    outb(idec->cmd_addr + IDE_REG_CMD, IDE_CMD_SETFEATURES);

    ide_wait(idec, IDE_STAT_DRDY, IDE_STAT_DRDY);
    if ((idec->ide_status & (IDE_STAT_DF | IDE_STAT_ERR)))
	cprintf("IDE: Unable to enable read look-ahead\n");

    disk_bytes = identify_buf.id.lba_sectors;
    disk_bytes *= 512;

    uint8_t bm_status = inb(idec->bm_addr + IDE_BM_STAT_REG);
    if (bm_status & IDE_BM_STAT_SIMPLEX)
	cprintf("Simplex-mode IDE bus master, potential problems later..\n");

    // Enable interrupts (clear the IDE_CTL_NIEN bit)
    outb(idec->ctl_addr, 0);

    idec->ih.ih_func = &ide_intr;
    irq_register(idec->irq, &idec->ih);
}

// Disk interface, from disk.h
uint64_t disk_bytes;

void
disk_init(struct pci_func *pcif)
{
    static int disk_init_done = 0;
    if (disk_init_done++) {
	cprintf("Additional IDE controllers found -- ignoring\n");
	return;
    }

    struct ide_channel *idec = &the_ide_channel;

    // Use the first IDE channel on the IDE controller
    idec->cmd_addr = pcif->reg_size[0] ? pcif->reg_base[0] : 0x1f0;
    idec->ctl_addr = pcif->reg_size[1] ? pcif->reg_base[1] + 2 : 0x3f6;
    idec->bm_addr = pcif->reg_base[4];
    idec->irq = 14;	// PCI IRQ routing is too complicated

    // Use the second IDE drive on the channel
    the_ide_drive = 1;

    // Now initialize the chosen drive/channel
    ide_init(idec, the_ide_drive);
}

int
disk_io(disk_op op, struct iovec *iov_buf, int iov_cnt,
	uint64_t byte_offset, disk_callback cb, void *cbarg)
{
    struct ide_channel *idec = &the_ide_channel;
    struct ide_op *curop = &idec->current_op;

    if (!SAFE_EQUAL(curop->op, op_none))
	return -E_BUSY;

    curop->op = op;
    curop->iov_buf = iov_buf;
    curop->iov_cnt = iov_cnt;
    curop->byte_offset = byte_offset;
    curop->cb = cb;
    curop->cbarg = cbarg;

    int r = ide_send(idec, the_ide_drive);
    if (r < 0)
	curop->op = op_none;
    return r;
}
