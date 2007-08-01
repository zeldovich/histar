#include <kern/lib.h>
#include <kern/arch.h>
#include <kern/netdev.h>
#include <kern/timer.h>
#include <dev/greth.h>
#include <dev/grethreg.h>
#include <dev/ambapp.h>
#include <dev/amba.h>

static const char greth_mac[6] = { 0x00, 0x5E, 0x00, 0x00, 0x00, 0x01 };

struct greth_card {
    struct greth_regs *regs;
    struct net_device netdev;

    struct greth_bd txbd[128] __attribute__((aligned (1024)));
    struct greth_bd rxbd[128] __attribute__((aligned (1024)));
};

static void
greth_set_mac(struct greth_regs *regs, const char *mac)
{
    int msb = mac[0] << 8 | mac[1];
    int lsb = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
    regs->esa_msb = msb;
    regs->esa_lsb = lsb;
    /* Make sure GRETH likes the mac */
    assert(regs->esa_msb == msb && regs->esa_lsb == lsb);
}

void
greth_init(void)
{
    static_assert(sizeof(struct greth_card) <= PGSIZE);

    struct amba_apb_device dev;
    int r = amba_apbslv_device(VENDOR_GAISLER, GAISLER_ETHMAC, &dev, 0);
    if (!r)
	return;
    struct greth_regs *regs = pa2kva(dev.start);
    
    struct greth_card *c;
    r = page_alloc((void **) &c);
    if (r < 0) {
	cprintf("greth_init: error %s\n", e2s(r));
	return;
    }
    memset(c, 0, PGSIZE);
    
    regs->control |= GRETH_RESET;
    uint64_t s = timer_user_nsec();
    while (regs->control & GRETH_RESET) {
	if ((timer_user_nsec() - s) > 1000000000) {
	    cprintf("greth_init: timeout waiting for reset\n");
	    return;
	}
    }
    
    greth_set_mac(regs, greth_mac);
    memcpy(&c->netdev.mac_addr[0], &greth_mac[0], 6);
    c->regs = regs;
    c->netdev.arg = c;
        
    cprintf("greth: mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	    c->netdev.mac_addr[0], c->netdev.mac_addr[1],
	    c->netdev.mac_addr[2], c->netdev.mac_addr[3],
	    c->netdev.mac_addr[4], c->netdev.mac_addr[5]);
}
