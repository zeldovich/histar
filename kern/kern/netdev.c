#include <kern/netdev.h>
#include <kern/lib.h>
#include <inc/error.h>

struct net_device *netdevs[netdevs_max];
uint64_t netdevs_num;

void
netdev_macaddr(struct net_device *ndev, uint8_t *addrbuf)
{
    memcpy(addrbuf, &ndev->mac_addr[0], 6);
}

int
netdev_add_buf(struct net_device *ndev, const struct Segment *sg,
	       uint64_t offset, netbuf_type type)
{
    return ndev->add_buf(ndev->arg, sg, offset, type);
}

int64_t
netdev_thread_wait(struct net_device *ndev, const struct Thread *t,
		   uint64_t waiter, int64_t gen)
{
    if (waiter != ndev->waiter_id) {
	ndev->buffer_reset(ndev->arg);
	ndev->waiter_id = waiter;
	ndev->wait_gen = 0;
	return -E_AGAIN;
    }

    if (gen != ndev->wait_gen)
	return ndev->wait_gen;

    thread_suspend(t, &ndev->wait_list);
    return 0;
}

void
netdev_thread_wakeup(struct net_device *ndev)
{
    ndev->wait_gen++;
    if (ndev->wait_gen <= 0)
	ndev->wait_gen = 1;

    while (!LIST_EMPTY(&ndev->wait_list)) {
	struct Thread *t = LIST_FIRST(&ndev->wait_list);
	thread_set_runnable(t);
    }
}

void
netdev_register(struct net_device *ndev)
{
    if (netdevs_num >= netdevs_max) {
	cprintf("netdev_register: out of netdev slots\n");
	return;
    }

    netdevs[netdevs_num++] = ndev;
}
