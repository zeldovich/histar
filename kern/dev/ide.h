#ifndef JOS_KERN_IDE_H
#define JOS_KERN_IDE_H

#include <dev/pci.h>

void ide_init(struct pci_func *pcif);

#endif
