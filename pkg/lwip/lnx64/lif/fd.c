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

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#include <lwip/opt.h>
#include <lwip/def.h>
#include <lwip/mem.h>
#include <lwip/pbuf.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>

#include <lif/init.h>
#include <lif/fd.h>

enum { fd_mtu = 1500 };

char mac_addr[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x10 };

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
    int the_fd = (int) (int64_t) netif->state;

    int txsize = 0;
    static char packet[fd_mtu];

    for (struct pbuf *q = p; q != NULL; q = q->next) {
	if (txsize + q->len > fd_mtu)
	    lwip_panic("oversized packet, fragment %d txsize %d\n", q->len, txsize);
	memcpy(&packet[txsize], q->payload, q->len);
	txsize += q->len;
    }

    lwip_core_unlock();
    int r = write(the_fd, packet, txsize);
    lwip_core_lock();
    if (r < 0)
	lwip_panic("write error: %s\n", strerror(errno));
    else if (r != txsize)
	lwip_panic("write trucated: %d -> %d\n", txsize, r); 
    
    return ERR_OK;
}

static err_t
fd_output(struct netif *netif, struct pbuf *p,
      struct ip_addr *ipaddr)
{
    /* resolve hardware address, then send (or queue) packet */
    return etharp_output(netif, ipaddr, p);
}

static void
low_level_init(struct netif *netif)
{
    netif->hwaddr_len = 6;
    netif->mtu = fd_mtu;
    netif->flags = NETIF_FLAG_BROADCAST;
    memcpy(&netif->hwaddr[0], mac_addr, 6);
}

err_t
fd_init(struct netif *netif)
{
    netif->output = fd_output;
    netif->linkoutput = low_level_output;
    memcpy(&netif->name[0], "ns", 2);

    low_level_init(netif);
    etharp_init();
    
    return ERR_OK;
}

void
fd_input(struct netif *netif)
{
    static char packet[fd_mtu];
    int the_fd = (int) (int64_t) netif->state;
    struct eth_hdr *ethhdr;
    
    lwip_core_unlock();
    ssize_t cc = read(the_fd, packet, fd_mtu);
    lwip_core_lock();
    
    if (cc < 0) {
	printf("fd_input: cannot read packet: %s\n", strerror(errno));
	return;
    }

    struct pbuf *p = pbuf_alloc(PBUF_RAW, cc, PBUF_POOL);
    if (p == 0) {
	printf("tun_input: pbuf_alloc out of memory\n");
	return;
    }
    
    int copied = 0;
    for (struct pbuf *q = p; q != NULL; q = q->next) {
	int bytes = q->len;
	if (bytes > (cc - copied))
	    bytes = cc - copied;
	memcpy(q->payload, &packet[copied], bytes);
	copied += bytes;
    }

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
	etharp_arp_input(netif, (struct eth_addr *)mac_addr, p);
	break;

    default:
	pbuf_free(p);
    }


    //netif->input(p, netif);
    return;
}

