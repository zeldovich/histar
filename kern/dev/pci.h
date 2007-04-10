#ifndef JOS_DEV_PCI_H
#define JOS_DEV_PCI_H

#include <machine/types.h>

// PCI subsystem interface
struct pci_bus;

struct pci_func {
    struct pci_bus *bus;

    uint32_t dev;
    uint32_t func;

    uint32_t reg_base[6];
    uint32_t reg_size[6];
    uint8_t irq_line;
};

struct pci_bus {
    struct pci_func *parent_bridge;
    uint32_t busno;

    uint32_t mem_base;		// First memory address
    uint32_t mem_limit;		// One past the last memory address

    uint32_t io_base;		// First IO address
    uint32_t io_limit;		// One past the last IO address
};

void pci_init(void);
void pci_func_enable(struct pci_func *f);

#endif
