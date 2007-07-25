#include <dev/ahci.h>
#include <dev/disk.h>
#include <dev/pcireg.h>
#include <dev/ahcireg.h>
#include <kern/arch.h>
#include <inc/error.h>

struct ahci_hba {
    uint32_t irq;
    uint32_t membase;
};

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

    static_assert(PGSIZE >= sizeof(*a));
    pci_func_enable(f);
    a->irq = f->irq_line;
    a->membase = f->reg_base[5];
    cprintf("AHCI: base 0x%x, irq %d\n", a->membase, a->irq);

    return 1;
}
