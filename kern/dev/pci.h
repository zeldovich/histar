#ifndef JOS_DEV_PCI_H
#define JOS_DEV_PCI_H

#include <machine/types.h>

// PCI subsystem interface
struct pci_func {
    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};

struct pci_bus {
    uint32_t mem_base;
    uint32_t mem_limit;

    uint32_t io_base;
    uint32_t io_limit;
};

void pci_init();

#endif
