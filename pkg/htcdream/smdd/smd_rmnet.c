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

//#define DEBUG WOOHOO

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
#include <inc/jhexdump.h>
}

#include "msm_smd.h"
#include "smd_rmnet.h"

#define NPKTQUEUE	64

struct rxpkt {
	char buf[1514];
	int bytes;
};

static struct rmnet_private {
	smd_channel_t *ch;
	const char *chname;
	size_t tx_frames, tx_frame_bytes;
	size_t rx_frames, rx_frame_bytes, rx_dropped;
	struct rxpkt rxq[NPKTQUEUE];
	int rxq_head;
	int rxq_tail;
	pthread_mutex_t mtx;
	pthread_cond_t  cond;
} rmnet_private[3];

static void smd_net_notify(void *_priv, unsigned event)
{
	if (event != SMD_EVENT_DATA)
		return;

	struct rmnet_private *p = (struct rmnet_private *)_priv;
	int sz;

	for (;;) {
		sz = smd_cur_packet_size(p->ch);
		if (sz == 0)
			break;
		if (smd_read_avail(p->ch) < sz)
			break;

		if (sz > 1514) {
			cprintf("rmnet_recv() discarding %d len\n", sz);
			if (smd_read(p->ch, NULL, sz) != sz)
				cprintf("rmnet_recv() smd lied about avail?!\n");
			p->rx_dropped++;
		} else {
			struct rxpkt *rxp = &p->rxq[p->rxq_head];

			if (smd_read(p->ch, rxp->buf, sz) != sz) {
				cprintf("rmnet_recv() smd lied about avail?!\n");
				return;
			}

#ifdef DEBUG
			cprintf("RMNET RECEIVED A FRAME (%d bytes)\n", sz);
			jhexdump((const unsigned char *)pktbuf, sz);
#endif

			p->rx_frames++;
			p->rx_frame_bytes += sz;

			pthread_mutex_lock(&p->mtx);
			rxp->bytes = sz;
			p->rxq_head = (p->rxq_head + 1) % NPKTQUEUE;
			if (p->rxq_head == p->rxq_tail)
				cprintf("RMNET OVERRAN THE WHOLE RING!!\n");
			pthread_cond_broadcast(&p->cond);
			pthread_mutex_unlock(&p->mtx);
		}
	}
}

extern "C" int smd_rmnet_open(int which)
{
	int r;

	if (which < 0 || which >= 3)
		return -E_NOT_FOUND;

#ifdef DEBUG
	cprintf("rmnet_open(%d)\n", which);
#endif

	struct rmnet_private *p = &rmnet_private[which]; 
	if (!p->ch) {
		r = smd_open(p->chname, &p->ch, p, smd_net_notify);

		if (r < 0)
			return r;
	}

	return 0;
}

extern "C" void smd_rmnet_close(int which)
{
}

extern "C" int smd_rmnet_xmit(int which, void *buf, int len)
{
	if (which < 0 || which >= 3)
		return -E_NOT_FOUND;

	struct rmnet_private *p = &rmnet_private[which]; 
	if (p->ch == NULL)
		return -E_INVAL;

#ifdef DEBUG
	cprintf("RMNET XMITTING A FRAME: which = %d, %d bytes\n", which, len);
	jhexdump((const unsigned char *)buf, len);
#endif

	// NB: smd_write already does mutual exclusion
	if (smd_write(p->ch, buf, len) != len) {
		cprintf("rmnet fifo full, dropping packet\n");
	} else {
		p->tx_frames++;
		p->tx_frame_bytes += len;	
	}

	return 0;
}

extern "C" int smd_rmnet_recv(int which, void *buf, int len)
{
	if (which < 0 || which >= 3)
		return -E_NOT_FOUND;

	struct rmnet_private *p = &rmnet_private[which]; 
	if (p->ch == NULL)
		return -E_INVAL;

	pthread_mutex_lock(&p->mtx);
	while (p->rxq_tail == p->rxq_head)
		pthread_cond_wait(&p->cond, &p->mtx);

	struct rxpkt *rxp = &p->rxq[p->rxq_tail];
	int recvd = MIN(len, rxp->bytes);
	memcpy(buf, rxp->buf, recvd);
	rxp->bytes = 0;
	p->rxq_tail = (p->rxq_tail + 1) % NPKTQUEUE;
	pthread_mutex_unlock(&p->mtx);

	return recvd;
}

extern "C" void smd_rmnet_init()
{
	static const char *names[3] = { "SMD_DATA5", "SMD_DATA6", "SMD_DATA7" };
	for (int i = 0; i < 3; i++) {
		struct rmnet_private *p = &rmnet_private[i]; 
		memset(p, 0, sizeof(*p));
		p->chname = names[i];
		pthread_mutex_init(&p->mtx, NULL);
		pthread_cond_init(&p->cond, NULL);
	}
}
