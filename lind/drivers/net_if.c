#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/platform_device.h>

#include <archcall.h>
#include <transport.h>

static const char xmit_dbg = 0;

#define DRIVER_NAME "lind-netdev"

static struct platform_device lind_pdev =  {
    .id = 0,
    .name = DRIVER_NAME,
};

static inline void 
set_ether_mac(struct net_device *dev, unsigned char *addr)
{
    memcpy(dev->dev_addr, addr, ETH_ALEN);
}

int
net_if_rx(struct net_device *dev)
{
    struct transport *dev_trans = dev->priv;
    int pkt_len;
    struct sk_buff *skb;
    int r;
    
    /* a couple extra bytes just in case */
    int skb_sz = dev->mtu + 100;
    /* If we can't allocate memory, try again next round. */
    skb = dev_alloc_skb(skb_sz);
    if (skb == NULL) {
	arch_printf("net_if_rx: out of memory\n");
	return 0;
    }
    
    skb->dev = dev;
    skb_put(skb, skb_sz);
    skb->mac.raw = skb->data;
    pkt_len = (*dev_trans->rx)(dev_trans->data, skb->mac.raw, skb_sz);
    if (pkt_len > 0) {
	skb_trim(skb, pkt_len);
	skb->protocol = eth_type_trans(skb, skb->dev);
	r = netif_rx(skb);
	if (xmit_dbg)
	    arch_printf("net_if_rx: len %ld, netif_rx %d\n", pkt_len, r);
	return pkt_len;
    }
    
    kfree_skb(skb);
    return pkt_len;
}

irqreturn_t
net_if_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct transport *dev_trans = dev->priv;
    int err;

    if(!netif_running(dev))
	return IRQ_NONE;
    
    spin_lock(&dev_trans->lock);
    while((err = net_if_rx(dev)) > 0);
    if (err < 0)
	arch_printf("net_if_interrupt: error %d\n", err);

    if (dev_trans->irq_reset)
	(*dev_trans->irq_reset)(dev_trans->data);
    
    spin_unlock(&dev_trans->lock);
    return IRQ_HANDLED;
}

static int 
net_if_close(struct net_device *dev)
{
    struct transport *dev_trans = dev->priv;
    netif_stop_queue(dev);
    free_irq(dev->irq, dev);

    if (dev_trans->close) {
	int r = (*dev_trans->close)(dev_trans->data);
	if (r < 0)
	    arch_printf("net_if_close: error %d\n", r);
    }
    return 0;
}

static int 
net_if_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    unsigned long flags;
    int len;
    struct transport *dev_trans;
    dev_trans = dev->priv;
    
    netif_stop_queue(dev);

    spin_lock_irqsave(&dev_trans->lock, flags);
    
    len = (*dev_trans->tx)(dev_trans->data, skb->data, skb->len);
    
    if (xmit_dbg)
	arch_printf("net_if_start_xmit: len %ld, actual %ld\n", skb->len, len);
    
    netif_start_queue(dev);
    if (len == skb->len) {
	dev->trans_start = jiffies;
	/* this is normally done in the interrupt when tx finishes */
	netif_wake_queue(dev);
	/* XXX keep some stats? */
    } else if(len == 0) {
	printk(KERN_INFO "net_if_start_xmit: xmit dropped\n");
    } else {
	printk(KERN_ERR "net_if_start_xmit: failed: %d\n", len);
    }

    spin_unlock_irqrestore(&dev_trans->lock, flags);
    
    dev_kfree_skb(skb);

    return 0;
}

static int 
net_if_open(struct net_device *dev)
{    
    int err;
    struct transport *dev_trans;
    dev_trans = dev->priv;
    
    err = (*dev_trans->open)(dev->name, dev_trans->data);
    if (err != 0) {
	printk(KERN_ERR "net_if_open: failed to open '%s': %d\n", 
	       dev_trans->name, err);
	return -ENETUNREACH;
    }
    
    err = request_irq(dev->irq, net_if_interrupt,
		      IRQF_DISABLED | IRQF_SHARED, dev->name, dev);
    if (err != 0) {
	printk(KERN_ERR "net_if_open: failed to get irq: %d\n", err);
	return -ENETUNREACH;
    }
        
    netif_start_queue(dev);

    printk(KERN_INFO "%s netdevice %s, %d, %02x:%02x:%02x:%02x:%02x:%02x\n", 
	   dev_trans->name, dev->name, dev->ifindex, 
	   dev->dev_addr[0], dev->dev_addr[1],
	   dev->dev_addr[2], dev->dev_addr[3], 
	   dev->dev_addr[4], dev->dev_addr[5]);

    return 0;
}

static void 
net_if_tx_timeout(struct net_device *dev)
{
    if (xmit_dbg)
	arch_printf("net_if_tx_timeout: ...\n");
    dev->trans_start = jiffies;
    netif_wake_queue(dev);
}

static int 
net_if_set_mac(struct net_device *dev, void *addr)
{
    struct transport *dev_trans = dev->priv;
    spin_lock_irq(&dev_trans->lock);
    memcpy(dev->dev_addr, addr, ETH_ALEN);
    spin_unlock_irq(&dev_trans->lock);
    return 0;
}

int
register_transport(struct transport *trans)
{
    struct net_device *dev;
    struct transport *dev_trans;
    int size, err;
    
    size = sizeof(*dev_trans) + trans->data_len;
    dev = alloc_etherdev(size);
    if (dev == NULL) {
	printk(KERN_ERR "register_if: failed to allocate device\n");
	return 1;
    }

    platform_device_register(&lind_pdev);
    SET_NETDEV_DEV(dev, &lind_pdev.dev);

    strncpy(dev->name, trans->name, IFNAMSIZ);
    dev->mtu = trans->mtu;
    dev->open = net_if_open;
    dev->stop = net_if_close;
    dev->hard_start_xmit = net_if_start_xmit;
    dev->tx_timeout = net_if_tx_timeout;
    dev->set_mac_address = net_if_set_mac;
    dev->irq = LIND_ETH_IRQ;
    dev_trans = dev->priv;
    memcpy(dev_trans, trans, sizeof(*dev_trans));
    spin_lock_init(&dev_trans->lock);
    
    rtnl_lock();
    err = register_netdevice(dev);
    rtnl_unlock();

    if (err) {
	free_netdev(dev);
	return 1;
    }

    memcpy(dev->dev_addr, dev_trans->mac, ETH_ALEN);
    return 0;
}
