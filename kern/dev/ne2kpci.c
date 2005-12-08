#include <dev/ne2kpci.h>
#include <kern/lib.h>

struct ne2kpci_card {
    bool_t running;

    uint8_t irq_line;
    void *iobase;
};

static struct ne2kpci_card the_card;

void
ne2kpci_attach(struct pci_func *pcif)
{
    struct ne2kpci_card *c = &the_card;

    c->irq_line = pcif->irq_line;
    c->io_addr = pcif->reg_base[0]
    if (pcif->reg_size[0] < 16) {
	cprintf("ne2k: io window too small: %d @ 0x%x\n",
		pcif->reg_size[0], pcif->reg_base[0]);
	return;
    }

    cprintf("ne2k: irq %d io 0x%x\n", pcif->irq_line, pcif->io_addr);

    //
    // XXX
    //
    // The problem is that NE2000 doesn't have any scatter-gather DMA
    // support, which is really useful for having a split kernel/user
    // network driver.
    //
    // We could do explicit copies from user buffer into NE2000 ring
    // and back, but it seems like a bit of a pain, so I'll try to
    // do the i82557 driver first.
    //
    cprintf("ne2k: not supported\n");
}
