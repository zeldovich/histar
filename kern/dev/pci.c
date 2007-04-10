#include <machine/x86.h>
#include <dev/pci.h>
#include <dev/pcireg.h>
#include <dev/disk.h>
#include <dev/ne2kpci.h>
#include <dev/fxp.h>
#include <dev/pnic.h>
#include <dev/e1000.h>
#include <kern/lib.h>

// Flag to do "lspci" at bootup
static int pci_show_devs = 0;

// PCI "configuration mechanism one"
static uint32_t pci_conf1_addr_ioport = 0x0cf8;
static uint32_t pci_conf1_data_ioport = 0x0cfc;

static void
pci_conf1_set_addr(uint32_t bus,
		   uint32_t dev,
		   uint32_t func,
		   uint32_t offset)
{
    assert(bus < 256);
    assert(dev < 32);
    assert(func < 8);
    assert(offset < 256);
    assert((offset & 0x3) == 0);

    uint32_t v = (1 << 31) |		// config-space
		 (bus << 16) | (dev << 11) | (func << 8) | (offset);
    outl(pci_conf1_addr_ioport, v);
}

static uint32_t
pci_conf_read(struct pci_func *f, uint32_t off)
{
    pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
    return inl(pci_conf1_data_ioport);
}

static void
pci_conf_write(struct pci_func *f, uint32_t off, uint32_t v)
{
    pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
    outl(pci_conf1_data_ioport, v);
}

// Bus configuration logic
static uint32_t
pci_alloc_aligned(uint32_t *addrp, uint32_t size)
{
    uint32_t mask = size - 1;
    *addrp = (*addrp + mask) & ~mask;
    uint32_t alloc = *addrp;
    *addrp += size;
    return alloc;
}

static void
pci_attach(uint32_t dev_id, uint32_t dev_class, struct pci_func *pcif)
{
    if (PCI_CLASS(dev_class) == PCI_CLASS_MASS_STORAGE &&
	PCI_SUBCLASS(dev_class) == PCI_SUBCLASS_MASS_STORAGE_IDE)
    {
	disk_init(pcif);
	return;
    }

    if (PCI_VENDOR(dev_id) == 0x10ec && PCI_PRODUCT(dev_id) == 0x8029) {
	ne2kpci_attach(pcif);
	return;
    }

    if (PCI_VENDOR(dev_id) == 0x8086 && PCI_PRODUCT(dev_id) == 0x1229) {
	fxp_attach(pcif);
	return;
    }

    if (PCI_VENDOR(dev_id) == 0xfefe && PCI_PRODUCT(dev_id) == 0xefef) {
	pnic_attach(pcif);
	return;
    }

    if (PCI_VENDOR(dev_id) == 0x8086 && PCI_PRODUCT(dev_id) == 0x107c) {
	e1000_attach(pcif);
	return;
    }
}

static void
pci_config_bus(struct pci_bus *bus)
{
    struct pci_func df;
    memset(&df, 0, sizeof(df));
    df.bus = bus;

    for (df.dev = 0; df.dev < 32; df.dev++) {
	uint32_t bhlc = pci_conf_read(&df, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) > 1)	    // Unsupported or no device
	    continue;

	if (PCI_HDRTYPE_TYPE(bhlc) == 1) {
	    cprintf("PCI-to-PCI bridge not supported yet, "
		    "some devices may be missing\n");
	    continue;
	}

	struct pci_func f = df;
	for (f.func = 0; f.func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1);
			 f.func++) {
	    uint32_t dev_id = pci_conf_read(&f, PCI_ID_REG);
	    if (PCI_VENDOR(dev_id) == 0xffff)
		continue;

	    struct pci_func af = f;
	    uint32_t intr = pci_conf_read(&af, PCI_INTERRUPT_REG);
	    af.irq_line = PCI_INTERRUPT_LINE(intr);

	    uint32_t dev_class = pci_conf_read(&af, PCI_CLASS_REG);
	    if (pci_show_devs)
		cprintf("PCI: %02x:%02x.%d: %04x:%04x: class %x.%x irq %d\n",
			af.bus->busno, af.dev, af.func,
			PCI_VENDOR(dev_id), PCI_PRODUCT(dev_id),
			PCI_CLASS(dev_class), PCI_SUBCLASS(dev_class),
			af.irq_line);

	    pci_attach(dev_id, dev_class, &af);
	}
    }

    // XXX
    // I would have thought that the bridge needs to be configured
    // with the base/limit addresses of the memory and IO space of
    // the PCI devices behind it.  But it seems to work without it
    // on both bochs and qemu.  If real hardware doesn't work, then
    // this is one place to look.
}

// External PCI subsystem interface

void
pci_func_enable(struct pci_func *f)
{
    pci_conf_write(f, PCI_COMMAND_STATUS_REG,
		   PCI_COMMAND_IO_ENABLE |
		   PCI_COMMAND_MEM_ENABLE |
		   PCI_COMMAND_MASTER_ENABLE);

    uint32_t bhlc = pci_conf_read(f, PCI_BHLC_REG);
    if (PCI_LATTIMER(bhlc) < 16)
	bhlc = (bhlc & ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT)) |
	       (64 << PCI_LATTIMER_SHIFT);
    //cprintf("PCI: %02x:%02x.%d: latency timer %d\n",
    //        f->bus->busno, f->dev, f->func, PCI_LATTIMER(bhlc));
    pci_conf_write(f, PCI_BHLC_REG, bhlc);

    uint32_t bar_width;
    for (uint32_t bar = PCI_MAPREG_START; bar < PCI_MAPREG_END;
	 bar += bar_width)
    {
	bar_width = 4;
	pci_conf_write(f, bar, 0xffffffff);
	uint32_t rv = pci_conf_read(f, bar);

	if (rv == 0)
	    continue;

	int regnum = PCI_MAPREG_NUM(bar);
	uint32_t base, size;
	if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
	    if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
		bar_width = 8;

	    size = PCI_MAPREG_MEM_SIZE(rv);
	    base = pci_alloc_aligned(&f->bus->mem_limit, size);
	    //cprintf("  mem region %d: %d bytes\n", regnum, size);
	} else {
	    size = PCI_MAPREG_IO_SIZE(rv);
	    base = pci_alloc_aligned(&f->bus->io_limit, size);
	    //cprintf("  io region %d: %d bytes\n", regnum, size);
	}

	pci_conf_write(f, bar, base);
	f->reg_base[regnum] = base;
	f->reg_size[regnum] = size;
    }
}

void
pci_init(void)
{
    static struct pci_bus root_bus;
    root_bus.parent_bridge = 0;
    root_bus.busno = 0;
    root_bus.mem_base = root_bus.mem_limit = 0xf0000000;
    root_bus.io_base = root_bus.io_limit = 0xc000;

    pci_config_bus(&root_bus);
}
