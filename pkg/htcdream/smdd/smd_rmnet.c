/* linux/drivers/net/msm_rmnet.c
 *
 * Virtual Ethernet Interface for MSM7K Networking
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/mman.h>

#include <inc/assert.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/queue.h>
#include <inc/syscall.h>
}

#include "msm_smd.h"
#include "smd_qmi.h"

/* XXX should come from smd headers */
#define SMD_PORT_ETHER0 11

struct rmnet_private {
	smd_channel_t *ch;
	const char *chname;
} rmnet_private[3] = {
	{ NULL, "SMD_DATA5" },
	{ NULL, "SMD_DATA6" },
	{ NULL, "SMD_DATA7" }
};

/* Called in soft-irq context */
static void smd_net_data_handler(unsigned long arg)
{
	struct net_device *dev = (struct net_device *) arg;
	struct rmnet_private *p = netdev_priv(dev);
	struct sk_buff *skb;
	void *ptr = 0;
	int sz;

	for (;;) {
		sz = smd_cur_packet_size(p->ch);
		if (sz == 0) break;
		if (smd_read_avail(p->ch) < sz) break;

		if (sz > 1514) {
			pr_err("rmnet_recv() discarding %d len\n", sz);
			ptr = 0;
		} else {
			skb = dev_alloc_skb(sz + NET_IP_ALIGN);
			if (skb == NULL) {
				pr_err("rmnet_recv() cannot allocate skb\n");
			} else {
				skb->dev = dev;
				skb_reserve(skb, NET_IP_ALIGN);
				ptr = skb_put(skb, sz);
				if (smd_read(p->ch, ptr, sz) != sz) {
					pr_err("rmnet_recv() smd lied about avail?!");
					ptr = 0;
					dev_kfree_skb_irq(skb);
				} else {
					skb->protocol = eth_type_trans(skb, dev);
					if (count_this_packet(ptr, skb->len)) {
						p->stats.rx_packets++;
						p->stats.rx_bytes += skb->len;
					}
					netif_rx(skb);
				}
				continue;
			}
		}
		if (smd_read(p->ch, ptr, sz) != sz)
			pr_err("rmnet_recv() smd lied about avail?!");
	}
}

static DECLARE_TASKLET(smd_net_data_tasklet, smd_net_data_handler, 0);

static void smd_net_notify(void *_dev, unsigned event)
{
	if (event != SMD_EVENT_DATA)
		return;

	smd_net_data_tasklet.data = (unsigned long) _dev;

	tasklet_schedule(&smd_net_data_tasklet);
}

static int rmnet_open(struct net_device *dev)
{
	int r;
	struct rmnet_private *p = netdev_priv(dev);

	cprintf("rmnet_open()\n");
	if (!p->ch) {
		r = smd_open(p->chname, &p->ch, dev, smd_net_notify);

		if (r < 0)
			return -ENODEV;
	}

	netif_start_queue(dev);
	return 0;
}

static int rmnet_stop(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);

	cprintf("rmnet_stop()\n");
	netif_stop_queue(dev);
	return 0;
}

static int rmnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	smd_channel_t *ch = p->ch;

	if (smd_write(ch, skb->data, skb->len) != skb->len) {
		pr_err("rmnet fifo full, dropping packet\n");
	} else {
		if (count_this_packet(skb->data, skb->len)) {
			p->stats.tx_packets++;
			p->stats.tx_bytes += skb->len;
		}
	}

	dev_kfree_skb_irq(skb);
	return 0;
}

static struct net_device_stats *rmnet_get_stats(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	return &p->stats;
}

static void __init rmnet_setup(struct net_device *dev)
{
	dev->open = rmnet_open;
	dev->stop = rmnet_stop;
	dev->hard_start_xmit = rmnet_xmit;
	dev->get_stats = rmnet_get_stats;
	dev->set_multicast_list = rmnet_set_multicast_list;
	dev->tx_timeout = rmnet_tx_timeout;

	dev->watchdog_timeo = 20; /* ??? */

	ether_setup(dev);

	dev->change_mtu = 0; /* ??? */

	random_ether_addr(dev->dev_addr);
}

static const char *ch_name[3] = {
	"SMD_DATA5",
	"SMD_DATA6",
	"SMD_DATA7",
};

static int __init rmnet_init(void)
{
	int ret;
	struct net_device *dev;
	struct rmnet_private *p;
	unsigned n;

	for (n = 0; n < 3; n++) {
		dev = alloc_netdev(sizeof(struct rmnet_private),
				   "rmnet%d", rmnet_setup);

		if (!dev)
			return -ENOMEM;

		p = netdev_priv(dev);
		p->chname = ch_name[n];

		ret = register_netdev(dev);
		if (ret) {
			free_netdev(dev);
			return ret;
		}
	}
	return 0;
}
