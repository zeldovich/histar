#include <dev/ahci.h>
#include <dev/disk.h>
#include <dev/pcireg.h>
#include <kern/arch.h>
#include <inc/error.h>

struct ahci_hba {
    uint32_t irq;
    uintptr_t membase;
};

void
ahci_init(struct pci_func *f)
{
    if (PCI_INTERFACE(f->dev_class) != 0x01) {
	cprintf("ahci_init: not an AHCI controller\n");
	return;
    }

    struct ahci_hba *a;
    int r = page_alloc((void **) &a);
    if (r < 0) {
	cprintf("ahci_init: cannot alloc page: %s\n", e2s(r));
	return;
    }

    static_assert(PGSIZE >= sizeof(*a));
    pci_func_enable(f);
    a->irq = f->irq_line;
    a->membase = f->reg_base[5];

    return;
}
