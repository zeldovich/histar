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
static int pci_show_addrs = 0;

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

    if (PCI_VENDOR(dev_id) == 0x8086 && (PCI_PRODUCT(dev_id) == 0x107c ||
					 PCI_PRODUCT(dev_id) == 0x109a)) {
	e1000_attach(pcif);
	return;
    }
}


static void
pci_bridge_update(struct pci_bus *sbus)		// sbus is secondary bus
{
    uint32_t busreg =
	sbus->parent_bridge->bus->busno << PCI_BRIDGE_BUS_PRIMARY_SHIFT   |
	sbus->base[pci_res_bus]		<< PCI_BRIDGE_BUS_SECONDARY_SHIFT |
	(sbus->end[pci_res_bus] - 1)	<< PCI_BRIDGE_BUS_SUBORDINATE_SHIFT;
    pci_conf_write(sbus->parent_bridge, PCI_BRIDGE_BUS_REG, busreg);

    uint32_t mem_base, mem_limit;
    if (sbus->base[pci_res_mem] < sbus->end[pci_res_mem]) {
	mem_base  = sbus->base[pci_res_mem];
	mem_limit = sbus->end[pci_res_mem] - 1;
    } else {
	mem_base  = 0xffffffff;
	mem_limit = 0;
    }

    uint32_t io_base, io_limit;
    if (sbus->base[pci_res_io] < sbus->end[pci_res_io]) {
	io_base  = sbus->base[pci_res_io];
	io_limit = sbus->end[pci_res_io] - 1;
    } else {
	io_base  = 0xffffffff;
	io_limit = 0;
    }

    uint32_t memreg =
	((mem_base >> 20) << PCI_BRIDGE_MEMORY_BASE_SHIFT) |
	((mem_limit >> 20) << PCI_BRIDGE_MEMORY_LIMIT_SHIFT);
    pci_conf_write(sbus->parent_bridge, PCI_BRIDGE_MEMORY_REG, memreg);

    uint32_t stat =
	pci_conf_read(sbus->parent_bridge, PCI_BRIDGE_STATIO_REG) &
	(PCI_BRIDGE_STATIO_STATUS_MASK << PCI_BRIDGE_STATIO_STATUS_SHIFT);
    uint32_t ioreg = stat |
	(((io_base >> 8) & PCI_BRIDGE_STATIO_IOBASE_MASK)
		<< PCI_BRIDGE_STATIO_IOBASE_SHIFT) |
	(((io_limit >> 8) & PCI_BRIDGE_STATIO_IOLIMIT_MASK)
		<< PCI_BRIDGE_STATIO_IOLIMIT_SHIFT);
    pci_conf_write(sbus->parent_bridge, PCI_BRIDGE_STATIO_REG, ioreg);
}

static uint32_t pci_bridge_units[pci_res_max] = {
    [pci_res_bus] = (1 << 0),	    // Allocate bus numbers one at a time
    [pci_res_mem] = (1 << 20),	    // Allocate memory in 1MB increments
    [pci_res_io]  = (1 << 12),	    // Allocate IO addresses in 4KB increments
};

static uint32_t
pci_bus_alloc(struct pci_bus *bus, uint32_t res, uint32_t size)
{
 retry:
    assert(res < pci_res_max);

    uint32_t mask = size - 1;
    uint32_t align = (bus->free[res] + mask) & ~mask;
    uint32_t nfree = align + size;
    if (nfree > bus->end[res]) {
	/* Out of resources on this bus, gotta ask the parent. */
	if (!bus->parent_bridge)
	    panic("pci_bus_alloc: out of resources: res %d, size %d\n",
		  res, size);

	uint32_t n = pci_bus_alloc(bus->parent_bridge->bus, res,
				   pci_bridge_units[res]);
	if (bus->base[res] == bus->end[res])
	    bus->base[res] = bus->free[res] = n;
	else
	    assert(n == bus->end[res]);
	bus->end[res] = n + pci_bridge_units[res];

	pci_bridge_update(bus);
	goto retry;
    }

    bus->free[res] = nfree;
    return align;
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
	    uint32_t ioreg = pci_conf_read(&df, PCI_BRIDGE_STATIO_REG);
	    if (PCI_BRIDGE_IO_32BITS(ioreg)) {
		cprintf("PCI: %02x:%02x: 32-bit bridge IO not supported.\n",
			df.bus->busno, df.dev);
		continue;
	    }

	    struct pci_bus nbus;
	    memset(&nbus, 0, sizeof(nbus));
	    nbus.parent_bridge = &df;
	    nbus.busno = pci_bus_alloc(bus, pci_res_bus, 1);
	    nbus.base[pci_res_bus] = nbus.busno;
	    nbus.free[pci_res_bus] = nbus.busno + 1;
	    nbus.end[pci_res_bus] = nbus.busno + 1;

	    if (pci_show_devs)
		cprintf("PCI: %02x:%02x.%d: bridge to PCI bus %d\n",
			df.bus->busno, df.dev, df.func, nbus.busno);

	    // Clear the ISA compat bit, we avoid ISA for simplicity..
	    uint32_t bctl = pci_conf_read(&df, PCI_BRIDGE_CONTROL_REG);
	    bctl &= ~(PCI_BRIDGE_CONTROL_ISA << PCI_BRIDGE_CONTROL_SHIFT);
	    pci_conf_write(&df, PCI_BRIDGE_CONTROL_REG, bctl);

	    pci_bridge_update(&nbus);
	    pci_conf_write(&df, PCI_COMMAND_STATUS_REG,
			   PCI_COMMAND_IO_ENABLE |
			   PCI_COMMAND_MEM_ENABLE |
			   PCI_COMMAND_MASTER_ENABLE);

	    pci_config_bus(&nbus);
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
	    base = pci_bus_alloc(f->bus, pci_res_mem, size);
	    if (pci_show_addrs)
		cprintf("  mem region %d: %d bytes at 0x%x\n",
			regnum, size, base);
	} else {
	    size = PCI_MAPREG_IO_SIZE(rv);
	    base = pci_bus_alloc(f->bus, pci_res_io, size);
	    if (pci_show_addrs)
		cprintf("  io region %d: %d bytes at 0x%x\n",
			regnum, size, base);
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
    memset(&root_bus, 0, sizeof(root_bus));

    /*
     * Note that this means we can't directly support over 3GB RAM.
     */
    root_bus.free[pci_res_bus] = 1;
    root_bus.base[pci_res_mem] = root_bus.free[pci_res_mem] = 0xc0000000;
    root_bus.base[pci_res_io]  = root_bus.free[pci_res_io]  = 0xc000;

    root_bus.end[pci_res_bus] = 0xff;
    root_bus.end[pci_res_mem] = 0xffffffff;
    root_bus.end[pci_res_io] = 0xffff;

    pci_config_bus(&root_bus);
}
