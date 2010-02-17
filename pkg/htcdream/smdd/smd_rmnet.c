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
#include "smd_rmnet.h"

struct rmnet_private {
	smd_channel_t *ch;
	const char *chname;
	size_t tx_frames, tx_frame_bytes;
	size_t rx_frames, rx_frame_bytes, rx_dropped;
	int pktbytes;
	char pktbuf[2000];
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
			char pktbuf[1514];
			if (smd_read(p->ch, pktbuf, sz) != sz) {
				cprintf("rmnet_recv() smd lied about avail?!\n");
			} else {
				p->rx_frames++;
				p->rx_frame_bytes += sz;	
				// XXX- do shit!
			}

			cprintf("RMNET RECEIVED A FRAME (%d bytes)\n", sz);

			pthread_mutex_lock(&p->mtx);
			if (p->pktbytes) {
				cprintf("PREVIOUS PACKET NOT YET READ (sz = %d)"
				    " - DROPPING!\n", p->pktbytes);
			}
			memcpy(p->pktbuf, pktbuf, sz); //XXX extra copy
			p->pktbytes = sz;
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

	cprintf("rmnet_open(%d)\n", which);

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
	while (p->pktbytes == 0)
		pthread_cond_wait(&p->cond, &p->mtx);
	memcpy(buf, p->pktbuf, MIN(len, p->pktbytes));
	p->pktbytes = 0;
	pthread_mutex_unlock(&p->mtx);

	return MIN(len, p->pktbytes);
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
