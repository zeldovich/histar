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

#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#include <sys/mman.h>

#include <jif/jif.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>

#include "netif/etharp.h"

#include <pkg/htcdream/smdd/msm_rpcrouter2.h>
#include <inc/smdd.h>
#include <pkg/htcdream/support/smddgate.h>

struct jif {
    struct eth_addr *ethaddr;
};

static void
low_level_init(struct netif *netif)
{
    //struct jif *jif = netif->state;

    netif->hwaddr_len = 6;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST;

    // Static MAC... Silicon Graphics OUI ;)
    netif->hwaddr[0] = 0x08;
    netif->hwaddr[1] = 0x00;
    netif->hwaddr[2] = 0x69;
    netif->hwaddr[3] = 0xbe;
    netif->hwaddr[4] = 0xef;
    netif->hwaddr[5] = 0xee;
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
    //struct jif *jif = netif->state;
    char buf[2000];

    int txsize = 0;

    for (struct pbuf *q = p; q != NULL; q = q->next) {
	if (txsize + q->len > 2000)
	    panic("oversized packet, fragment %d txsize %d\n", q->len, txsize);
	memcpy(&buf[txsize], q->payload, q->len);
	txsize += q->len;
    }

    smddgate_rmnet_tx(0, buf, txsize);

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
    int len = 2000;
    /* We allocate a pbuf chain of pbufs from the pool. */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p == 0) {
#if LINK_STATS
	lwip_stats.link.memerr++;
	lwip_stats.link.drop++;
#endif /* LINK_STATS */      
	return 0;
    }

    /* grab a packet from smdd via the receive gate */
    char buf[len];
    len = smddgate_rmnet_rx(0, buf, len);
    if (len <= 0 || len > 2000) {
        fprintf(stderr, "%s: invalid rmnet_rx packet length = %d\n", len);
        return NULL;
    }

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    int copied = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
	/* Read enough bytes to fill this pbuf in the chain. The
	 * available data in the pbuf is given by the q->len
	 * variable. */
	int bytes = q->len;
	if (bytes > (len - copied))
	    bytes = len - copied;
	memcpy(q->payload, buf + copied, bytes);
	copied += bytes;
    }

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
  
    p = low_level_input(netif);		/* move received packet into a new pbuf */
    if (p == NULL) return;		/* no packet could be read, silently ignore this */
    ethhdr = p->payload;		/* points to packet payload, which starts with an Ethernet header */

#if LINK_STATS
    lwip_stats.link.recv++;
#endif /* LINK_STATS */

    ethhdr = p->payload;

    switch (htons(ethhdr->type)) {
    case ETHTYPE_IP:
	etharp_ip_input(netif, p);			/* update ARP table */
	pbuf_header(p, -(int)sizeof(struct eth_hdr));	/* skip Ethernet header */
	netif->input(p, netif);				/* pass to network layer */
	break;
      
    case ETHTYPE_ARP:
	etharp_arp_input(netif, jif->ethaddr, p);	/* pass p to ARP module  */
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

    if (smddgate_init() != 0) {
	fprintf(stderr, "%s: smddgate_init err - is smdd running?\n", __func__);
	return ERR_MEM; // meh
    } 

    jif = mem_malloc(sizeof(struct jif));
    if (jif == NULL) {
	LWIP_DEBUGF(NETIF_DEBUG, ("jif_init: out of memory\n"));
	return ERR_MEM;
    }

    netif->state = jif;
    netif->output = jif_output;
    netif->linkoutput = low_level_output;
    memcpy(&netif->name[0], "en", 2);

    jif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

    low_level_init(netif);

    etharp_init();

    fprintf(stderr, "%s done - HTC Dream lwip interface up\n", __func__);

    return ERR_OK;
}
