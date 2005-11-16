#include <dev/pci.h>
#include <dev/pcireg.h>
#include <dev/disk.h>
#include <kern/lib.h>
#include <machine/x86.h>

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
pci_conf_read(uint32_t bus, uint32_t dev, uint32_t func, uint32_t off)
{
    pci_conf1_set_addr(bus, dev, func, off);
    return inl(pci_conf1_data_ioport);
}

static void
pci_conf_write(uint32_t bus, uint32_t dev, uint32_t func, uint32_t off, uint32_t v)
{
    pci_conf1_set_addr(bus, dev, func, off);
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
pci_attach(uint32_t id, uint32_t class, struct pci_func *pcif)
{
    if (PCI_CLASS(class) == PCI_CLASS_MASS_STORAGE &&
	PCI_SUBCLASS(class) == PCI_SUBCLASS_MASS_STORAGE_IDE)
    {
	disk_init(pcif);
    }
}

static void
pci_config_bus(uint32_t busno, struct pci_bus *bus)
{
    uint32_t dev, func, bar;

    for (dev = 0; dev < 32; dev++) {
	uint32_t bhlc = pci_conf_read(busno, dev, 0, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) > 1)	    // Unsupported (or non-present) device
	    continue;

	if (PCI_HDRTYPE_TYPE(bhlc) == 1) {
	    cprintf("PCI-to-PCI bridge not supported yet, some devices may be missing\n");
	    continue;
	}

	for (func = 0; func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1); func++) {
	    struct pci_func pcif;
	    memset(&pcif, 0, sizeof(pcif));

	    uint32_t id = pci_conf_read(busno, dev, func, PCI_ID_REG);
	    if (PCI_VENDOR(id) == 0xffff)
		continue;

	    uint32_t class = pci_conf_read(busno, dev, func, PCI_CLASS_REG);
	    cprintf("PCI: %02x:%02x.%d: %04x:%04x: class %x.%x ifa %x\n",
		    busno, dev, func,
		    PCI_VENDOR(id), PCI_PRODUCT(id),
		    PCI_CLASS(class), PCI_SUBCLASS(class), PCI_INTERFACE(class));

	    uint32_t bar_width;
	    for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END; bar += bar_width) {
		bar_width = 4;
		pci_conf_write(busno, dev, func, bar, 0xffffffff);
		uint32_t rv = pci_conf_read(busno, dev, func, bar);

		if (rv == 0)
		    continue;

		int regnum = PCI_MAPREG_NUM(bar);
		uint32_t base, size;
		if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
		    if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
			bar_width = 8;

		    size = PCI_MAPREG_MEM_SIZE(rv);
		    base = pci_alloc_aligned(&bus->mem_limit, size);
		    //cprintf("  mem region %d: %d bytes\n", regnum, size);
		} else {
		    size = PCI_MAPREG_IO_SIZE(rv);
		    base = pci_alloc_aligned(&bus->io_limit, size);
		    //cprintf("  io region %d: %d bytes\n", regnum, size);
		}

		pci_conf_write(busno, dev, func, bar, base);
		pcif.reg_base[regnum] = base;
		pcif.reg_size[regnum] = size;
	    }

	    pci_attach(id, class, &pcif);
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
static struct pci_bus root_bus;

void pci_init() {
    root_bus.mem_base = root_bus.mem_limit = 0xf0000000;
    root_bus. io_base = root_bus. io_limit = 0xc000;

    pci_config_bus(0, &root_bus);
}
