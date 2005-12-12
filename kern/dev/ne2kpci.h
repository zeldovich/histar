#ifndef JOS_KERN_DEV_NE2KPCI_H
#define JOS_KERN_DEV_NE2KPCI_H

#include <dev/pci.h>

void ne2kpci_attach(struct pci_func *pcif);
void ne2kpci_intr(void);

#endif
