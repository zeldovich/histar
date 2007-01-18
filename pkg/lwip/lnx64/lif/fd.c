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

#include <lif/init.h>
#include <lif/fd.h>

enum { fd_mtu = 1500 };

static err_t
fd_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr)
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

    struct tun_if *tun = netif->state;
    lwip_core_unlock();
    write(the_fd, packet, txsize);
    lwip_core_lock();

    return ERR_OK;
}

err_t
fd_init(struct netif *netif)
{
    memcpy(&netif->name[0], "ns", 2);
    netif->output = fd_output;
    netif->mtu = fd_mtu;
    //netif->flags = NETIF_FLAG_POINTTOPOINT;
    return ERR_OK;
}

void
fd_input(struct netif *netif)
{
    static char packet[fd_mtu];
    int the_fd = (int) (int64_t) netif->state;
    
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

    netif->input(p, netif);
    return;
}

