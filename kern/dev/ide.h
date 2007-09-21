#ifndef JOS_KERN_IDE_H
#define JOS_KERN_IDE_H

#include <dev/pci.h>

int ide_init(struct pci_func *pcif);

#endif
