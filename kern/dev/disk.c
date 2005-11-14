#include <dev/disk.h>

// PIO-based driver, copied from jos/fs/ide.c

#include <machine/x86.h>
#include <kern/lib.h>
#include <dev/ide.h>
#include <inc/error.h>

// first controller (compat mode), second device
static uint32_t ide_cmd_addr = 0x1f0;
//static uint32_t ide_ctl_addr = 0x3f6;
//static uint32_t ide_irq = 14;
static uint32_t ide_diskno = 1;

static int
ide_wait_ready()
{
    int r;

    for (;;) {
	r = inb(ide_cmd_addr + IDE_REG_STATUS);
	if ((r & (IDE_STAT_BSY | IDE_STAT_DRDY)) == IDE_STAT_DRDY)
	    break;
    }

    if ((r & (IDE_STAT_DF | IDE_STAT_ERR)))
	return -1;
    return 0;
}

static void
ide_select_sectors(uint32_t start_sector, uint32_t num_sectors)
{
    assert(num_sectors <= 256);

    // 28-bit addressing mode
    outb(ide_cmd_addr + IDE_REG_SECTOR_COUNT, num_sectors & 0xff);
    outb(ide_cmd_addr + IDE_REG_LBA_LOW, start_sector & 0xff);
    outb(ide_cmd_addr + IDE_REG_LBA_MID, (start_sector >> 8) & 0xff);
    outb(ide_cmd_addr + IDE_REG_LBA_HI, (start_sector >> 16) & 0xff);
    outb(ide_cmd_addr + IDE_REG_DEVICE, IDE_DEV_LBA |
					(ide_diskno << 4) |
					((start_sector >> 24) & 0x0f));
}

static int
ide_pio_in(void *buf, uint32_t num_sectors)
{
    for (; num_sectors > 0; num_sectors--, buf += 512) {
	int r = ide_wait_ready();
	if (r < 0)
	    return r;

	insl(ide_cmd_addr + IDE_REG_DATA, buf, 512 / 4);
    }

    return 0;
}

static int
ide_pio_out(void *buf, uint32_t num_sectors)
{
    for (; num_sectors > 0; num_sectors--, buf += 512) {
	int r = ide_wait_ready();
	if (r < 0)
	    return r;

	outsl(ide_cmd_addr + IDE_REG_DATA, buf, 512 / 4);
    }

    return 0;
}

// Outstanding command queue (1 deep for vanilla ATA)
static struct {
    disk_op op;
    void *buf;
    uint32_t num_bytes;
    uint64_t byte_offset;
    disk_callback cb;
    void *cbarg;
} current_op;

static void
ide_complete(disk_io_status stat)
{
    current_op.op = op_none;
    current_op.cb(stat,
		  current_op.buf,
		  current_op.num_bytes,
		  current_op.byte_offset,
		  current_op.cbarg);
}

static void
ide_send()
{
    uint32_t num_sectors = current_op.num_bytes / 512;

    ide_wait_ready();
    ide_select_sectors(current_op.byte_offset / 512, num_sectors);

    int r;
    if (current_op.op == op_write) {
	outb(ide_cmd_addr + IDE_REG_CMD, IDE_CMD_WRITE);
	r = ide_pio_out(current_op.buf, num_sectors);
    } else {
	outb(ide_cmd_addr + IDE_REG_CMD, IDE_CMD_READ);
	r = ide_pio_in(current_op.buf, num_sectors);
    }

    ide_complete(r == 0 ? disk_io_success : disk_io_failure);
}

/*static*/ void
ide_intr()
{
    // Ack IRQ by reading the status register
    int r = inb(ide_cmd_addr + IDE_REG_STATUS);
    if ((r & IDE_STAT_BSY)) {
	// Not yet?
	return;
    }

    if ((r & (IDE_STAT_DF | IDE_STAT_ERR))) {
	ide_complete(disk_io_failure);
	return;
    }

    if (current_op.op == op_write) {
	ide_complete(disk_io_success);
    } else {
	r = ide_pio_in(current_op.buf, current_op.num_bytes / 512);
	ide_complete(r == 0 ? disk_io_success : disk_io_failure);
    }
}

// Disk interface, from disk.h
uint64_t disk_bytes;
static union {
    struct identify_device id;
    char buf[512];
} identify_buf;

static void
ide_string_shuffle(char *s, int len)
{
    int i;
    for (i = 0; i < len; i += 2) {
	char c = s[i+1];
	s[i+1] = s[i];
	s[i] = c;
    }
}

void disk_init() {
    ide_wait_ready();

    outb(ide_cmd_addr + IDE_REG_DEVICE, ide_diskno << 4);
    outb(ide_cmd_addr + IDE_REG_CMD, IDE_CMD_IDENTIFY);

    if (ide_pio_in(&identify_buf, 1) < 0) {
	cprintf("Unable to identify disk device\n");
    } else {
	ide_string_shuffle(identify_buf.id.serial, sizeof(identify_buf.id.serial));
	ide_string_shuffle(identify_buf.id.model, sizeof(identify_buf.id.model));
	ide_string_shuffle(identify_buf.id.firmware, sizeof(identify_buf.id.firmware));

	cprintf("IDE device (%d sectors): %1.40s\n",
		identify_buf.id.lba_sectors,
		identify_buf.id.model);

	disk_bytes = identify_buf.id.lba_sectors;
	disk_bytes *= 512;
    }
}

int disk_io(disk_op op, void *buf,
	    uint32_t num_bytes, uint64_t byte_offset,
	    disk_callback cb, void *cbarg)
{
    if (current_op.op != op_none) {
	cprintf("Disk busy, dropping IO request?\n");
	return -E_BUSY;
    }

    current_op.op = op;
    current_op.buf = buf;
    current_op.num_bytes = num_bytes;
    current_op.byte_offset = byte_offset;
    current_op.cb = cb;
    current_op.cbarg = cbarg;

    ide_send();
    return 0;
}

// Some test routines..
static char disk_test_buf[512];
static int disk_test_count;
static char *disk_test_string = "hello disk.";

static void
disk_test_read_cb(disk_io_status s, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (s != disk_io_success) {
	cprintf("Unable to read from disk.\n");
    } else {
	cprintf("Reading from disk: %s\n", disk_test_buf);
	if (disk_test_count++ == 0) {
	    disk_test_string = "hello disk again.";
	    disk_test();
	}
    }
}

static void
disk_test_write_cb(disk_io_status s, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (s != disk_io_success) {
	cprintf("Unable to write to disk.\n");
    } else {
	memset(disk_test_buf, 0, 512);
	disk_io(op_read, disk_test_buf, 512, 0, disk_test_read_cb, 0);
    }
}

void
disk_test()
{
    memcpy(disk_test_buf, disk_test_string, strlen(disk_test_string) + 1);
    disk_io(op_write, disk_test_buf, 512, 0, disk_test_write_cb, 0);
}
