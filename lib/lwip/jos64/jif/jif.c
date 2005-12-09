/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/memlayout.h>
#include <inc/error.h>

#include <jif/jif.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>

#include "netif/etharp.h"

#include "jif.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define JIF_BUFS	8

struct jif {
    struct eth_addr *ethaddr;
    int64_t waiter_id;
    int64_t waitgen;

    struct netbuf_hdr *rx[JIF_BUFS];
    struct netbuf_hdr *tx[JIF_BUFS];

    struct cobj_ref buf_seg;
    void *buf_base;
};

static const struct eth_addr ethbroadcast =
	{{0xff,0xff,0xff,0xff,0xff,0xff}};

static void
low_level_init(struct netif *netif)
{
    netif->hwaddr_len = 6;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST;

    int r = sys_net_macaddr(&netif->hwaddr[0]);
    if (r < 0)
	panic("jif: cannot read MAC address");

    // container gets passed as the argument to _start()
    uint64_t container = start_arg;

    // Allocate transmit/receive pages
    struct jif *jif = netif->state;
    jif->waitgen = -1;
    jif->waiter_id = thread_id(container);
    if (jif->waiter_id < 0)
	panic("jif: cannot get thread id: %s", e2s(jif->waiter_id));
  
    r = segment_alloc(container, JIF_BUFS * PGSIZE, &jif->buf_seg);
    if (r < 0)
	panic("jif: cannot allocate %d buffer pages: %s\n",
	      JIF_BUFS, e2s(r));

    r = segment_map(container, jif->buf_seg, 1, &jif->buf_base, 0);
    if (r < 0)
	panic("jif: canont map %d buffer pages: %s\n",
	      JIF_BUFS, e2s(r));

    for (int i = 0; i < JIF_BUFS; i++) {
	jif->rx[i] = jif->buf_base + i * PGSIZE;
	jif->rx[i]->size = 2000;
	jif->rx[i]->actual_count = -1;

	jif->tx[i] = jif->buf_base + i * PGSIZE + 2048;
	jif->tx[i]->actual_count = NETHDR_COUNT_DONE;
    }
}

/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
    struct jif *jif = netif->state;

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE);			/* drop the padding word */
#endif

    int txslot;
    int warned = 0;
    do {
	for (txslot = 0; txslot < JIF_BUFS; txslot++) {
	    if ((jif->tx[txslot]->actual_count & NETHDR_COUNT_DONE))
		break;
	}

	if (txslot == JIF_BUFS) {
	    if (!warned++)
		cprintf("jif: out of tx bufs\n");
	    sys_thread_yield();
	}
    } while (txslot == JIF_BUFS);

    char *txbuf = (char *) (jif->tx[txslot] + 1);
    int txsize = 0;

    for (struct pbuf *q = p; q != NULL; q = q->next) {
	/* Send the data from the pbuf to the interface, one pbuf at a
	   time. The size of the data in each pbuf is kept in the ->len
	   variable. */

	if (txsize + q->len > 2000)
	    panic("oversized packet, fragment %d txsize %d\n", q->len, txsize);
	memcpy(&txbuf[txsize], q->payload, q->len);
	txsize += q->len;
    }

    void *txbase = jif->tx[txslot];
    jif->tx[txslot]->size = txsize;
    jif->tx[txslot]->actual_count = 0;

    warned = 0;
    for (;;) {
	int r = sys_net_buf(jif->buf_seg,
			    (uint64_t) (txbase - jif->buf_base),
			    netbuf_tx);
	if (r >= 0)
	    break;

	if (!warned++)
	    cprintf("jif: can't setup tx slot: %s\n", e2s(r));
	sys_thread_yield();
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);			/* reclaim the padding word */
#endif

#if LINK_STATS
    lwip_stats.link.xmit++;
#endif /* LINK_STATS */

    return ERR_OK;
}

/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */

static struct pbuf *
low_level_input(struct netif *netif)
{
    struct jif *jif = netif->state;

    int rxslot;
    do {
	for (rxslot = 0; rxslot < JIF_BUFS; rxslot++) {
	    if (jif->rx[rxslot]->actual_count == (uint16_t) -1) {
		void *bufaddr = jif->rx[rxslot];

		jif->rx[rxslot]->actual_count = 0;
		int r = sys_net_buf(jif->buf_seg,
				    (uint64_t) (bufaddr - jif->buf_base),
				    netbuf_rx);
		if (r < 0) {
		    cprintf("jif: cannot feed rx packet: %s\n",
			    e2s(r));
		    jif->rx[rxslot]->actual_count = -1;
		}
	    } else if ((jif->rx[rxslot]->actual_count & NETHDR_COUNT_DONE)) {
		break;
	    }
	}

	if (rxslot == JIF_BUFS) {
	    jif->waitgen = sys_net_wait(jif->waiter_id, jif->waitgen);
	    if (jif->waitgen == -E_AGAIN) {
		// All buffers have been cleared
		for (int i = 0; i < JIF_BUFS; i++) {
		    jif->rx[i]->actual_count = (uint16_t) -1;
		    jif->tx[i]->actual_count = NETHDR_COUNT_DONE;
		}
	    }
	}
    } while (rxslot == JIF_BUFS);

    uint16_t count = jif->rx[rxslot]->actual_count;
    jif->rx[rxslot]->actual_count = -1;

    if ((count & NETHDR_COUNT_ERR)) {
	cprintf("jif: rx packet error\n");
	return 0;
    }

    s16_t len = count & NETHDR_COUNT_MASK;

#if ETH_PAD_SIZE
    /* allow room for Ethernet padding */
    len += ETH_PAD_SIZE;
#endif

    /* We allocate a pbuf chain of pbufs from the pool. */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p == 0) {
#if LINK_STATS
	lwip_stats.link.memerr++;
	lwip_stats.link.drop++;
#endif /* LINK_STATS */      
	return 0;
    }

#if ETH_PAD_SIZE
    /* drop the padding word */
    pbuf_header(p, -ETH_PAD_SIZE);
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    void *rxbuf = (void *) (jif->rx[rxslot] + 1);
    int copied = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
	/* Read enough bytes to fill this pbuf in the chain. The
	 * available data in the pbuf is given by the q->len
	 * variable. */
	int bytes = q->len;
	if (bytes > (len - copied))
	    bytes = len - copied;
	memcpy(q->payload, rxbuf + copied, bytes);
	copied += bytes;
    }

#if ETH_PAD_SIZE
    /* reclaim the padding word */
    pbuf_header(p, ETH_PAD_SIZE);
#endif

#if LINK_STATS
    lwip_stats.link.recv++;
#endif /* LINK_STATS */

    return p;  
}

/*
 * jif_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actual transmission of the packet.
 *
 */

static err_t
jif_output(struct netif *netif, struct pbuf *p,
      struct ip_addr *ipaddr)
{
    /* resolve hardware address, then send (or queue) packet */
    return etharp_output(netif, ipaddr, p);
}

/*
 * jif_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface.
 *
 */

void
jif_input(struct netif *netif)
{
    struct jif *jif;
    struct eth_hdr *ethhdr;
    struct pbuf *p;

    jif = netif->state;
  
    /* move received packet into a new pbuf */
    p = low_level_input(netif);
    /* no packet could be read, silently ignore this */
    if (p == NULL) return;
    /* points to packet payload, which starts with an Ethernet header */
    ethhdr = p->payload;

#if LINK_STATS
    lwip_stats.link.recv++;
#endif /* LINK_STATS */

    ethhdr = p->payload;

    switch (htons(ethhdr->type)) {
    case ETHTYPE_IP:
	/* update ARP table */
	etharp_ip_input(netif, p);
	/* skip Ethernet header */
	pbuf_header(p, -(int)sizeof(struct eth_hdr));
	/* pass to network layer */
	netif->input(p, netif);
	break;
      
    case ETHTYPE_ARP:
	/* pass p to ARP module  */
	etharp_arp_input(netif, jif->ethaddr, p);
	break;

    default:
	pbuf_free(p);
    }
}

/*
 * jif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */

err_t
jif_init(struct netif *netif)
{
    struct jif *jif;

    jif = mem_malloc(sizeof(struct jif));

    if (jif == NULL) {
	LWIP_DEBUGF(NETIF_DEBUG, ("jif_init: out of memory\n"));
	return ERR_MEM;
    }

    netif->state = jif;
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;
    netif->output = jif_output;
    netif->linkoutput = low_level_output;

    jif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

    low_level_init(netif);

    etharp_init();

    return ERR_OK;
}
