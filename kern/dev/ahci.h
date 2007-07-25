#ifndef JOS_KERN_AHCI_H
#define JOS_KERN_AHCI_H

#include <dev/pci.h>

void ahci_init(struct pci_func *pcif);

#endif
