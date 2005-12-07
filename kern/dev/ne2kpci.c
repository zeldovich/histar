#include <dev/ne2kpci.h>
#include <kern/lib.h>

void
ne2kpci_attach(struct pci_func *pcif)
{
    cprintf("ne2kpci_attach @ irq %d\n", pcif->irq_line);
}
