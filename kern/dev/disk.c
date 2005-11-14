#include <dev/disk.h>

// PIO-based driver, copied from jos/fs/ide.c

#include <machine/x86.h>
#include <kern/lib.h>

#define IDE_BSY		0x80
#define IDE_DRDY	0x40
#define IDE_DF		0x20
#define IDE_ERR		0x01

static int diskno = 1;

static int
ide_wait_ready(bool check_error)
{
	int r;

	while (((r = inb(0x1F7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
		/* do nothing */;

	if (check_error && (r & (IDE_DF|IDE_ERR)) != 0)
		return -1;
	return 0;
}

bool
ide_probe_disk1(void)
{
	int r, x;

	// wait for Device 0 to be ready
	ide_wait_ready(0);

	// switch to Device 1
	outb(0x1F6, 0xE0 | (1<<4));

	// check for Device 1 to be ready for a while
	for (x = 0; x < 1000 && (r = inb(0x1F7)) == 0; x++)
		/* do nothing */;

	// switch back to Device 0
	outb(0x1F6, 0xE0 | (0<<4));

	cprintf("Device 1 presence: %d\n", (x < 1000));
	return (x < 1000);
}

int
ide_read(uint32_t secno, void *dst, size_t nsecs)
{
	int r;

	assert(nsecs <= 256);

	ide_wait_ready(0);

	outb(0x1F2, nsecs);
	outb(0x1F3, secno & 0xFF);
	outb(0x1F4, (secno >> 8) & 0xFF);
	outb(0x1F5, (secno >> 16) & 0xFF);
	outb(0x1F6, 0xE0 | ((diskno&1)<<4) | ((secno>>24)&0x0F));
	outb(0x1F7, 0x20);	// CMD 0x20 means read sector

	for (; nsecs > 0; nsecs--, dst += 512) {
		if ((r = ide_wait_ready(1)) < 0)
			return r;
		insl(0x1F0, dst, 512/4);
	}

	return 0;
}

int
ide_write(uint32_t secno, const void *src, size_t nsecs)
{
	int r;

	assert(nsecs <= 256);

	ide_wait_ready(0);

	outb(0x1F2, nsecs);
	outb(0x1F3, secno & 0xFF);
	outb(0x1F4, (secno >> 8) & 0xFF);
	outb(0x1F5, (secno >> 16) & 0xFF);
	outb(0x1F6, 0xE0 | ((diskno&1)<<4) | ((secno>>24)&0x0F));
	outb(0x1F7, 0x30);	// CMD 0x30 means write sector

	for (; nsecs > 0; nsecs--, src += 512) {
		if ((r = ide_wait_ready(1)) < 0)
			return r;
		outsl(0x1F0, src, 512/4);
	}

	return 0;
}

// Disk interface, from disk.h
uint64_t disk_bytes;
void disk_init() {
    if (!ide_probe_disk1())
	panic("no data disk present\n");

    disk_bytes = 0xdeadbeef;
}

int disk_read(void *buf, uint32_t count, uint64_t offset, disk_callback cb, void *cbarg) {
    int r;

    r = ide_read(offset / 512, buf, count / 512);
    cb(r == 0 ? disk_io_success : disk_io_failure, buf, count, offset, cbarg);
    return 0;
}

int disk_write(void *buf, uint32_t count, uint64_t offset, disk_callback cb, void *cbarg) {
    int r;

    r = ide_write(offset / 512, buf, count / 512);
    cb(r == 0 ? disk_io_success : disk_io_failure, buf, count, offset, cbarg);
    return 0;
}

// Some test routines..
char disk_test_buf[512];

static void
disk_test_read_cb(disk_io_status s, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (s != disk_io_success) {
	cprintf("Unable to read from disk.\n");
    } else {
	cprintf("Reading from disk: %s\n", disk_test_buf);
    }
}

static void
disk_test_write_cb(disk_io_status s, void *buf, uint32_t count, uint64_t offset, void *arg)
{
    if (s != disk_io_success) {
	cprintf("Unable to write to disk.\n");
    } else {
	memset(disk_test_buf, 0, 512);
	disk_read(disk_test_buf, 512, 0, disk_test_read_cb, 0);
    }
}

void
disk_test()
{
    memcpy(disk_test_buf, "hello disk.", 12);
    disk_write(disk_test_buf, 512, 0, disk_test_write_cb, 0);
}
