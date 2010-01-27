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
} rmnet_private[3] = {
	{ NULL, "SMD_DATA5", 0, 0, 0, 0, 0 },
	{ NULL, "SMD_DATA6", 0, 0, 0, 0, 0 },
	{ NULL, "SMD_DATA7", 0, 0, 0, 0, 0 }
};



#include <ctype.h>
static void
hexdump(const unsigned char *buf, unsigned int len)
{
	unsigned int i, j;

	i = 0;
	while (i < len) {
		char offset[9];
		char hex[16][3];
		char ascii[17];

		snprintf(offset, sizeof(offset), "%08x  ", i);
		offset[sizeof(offset) - 1] = '\0';

		for (j = 0; j < 16; j++) {
			if ((i + j) >= len) {
				strcpy(hex[j], "  ");
				ascii[j] = '\0';
			} else {
				snprintf(hex[j], sizeof(hex[0]), "%02x",
				    buf[i + j]);
				hex[j][sizeof(hex[0]) - 1] = '\0';
				if (isprint((int)buf[i + j]))
					ascii[j] = buf[i + j];
				else
					ascii[j] = '.';
			}
		}
		ascii[sizeof(ascii) - 1] = '\0';

		printf("%s  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s  "
		    "|%s|\n", offset, hex[0], hex[1], hex[2], hex[3], hex[4],
		    hex[5], hex[6], hex[7], hex[8], hex[9], hex[10], hex[11],
		    hex[12], hex[13], hex[14], hex[15], ascii);

		i += 16;
	}
}


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
				cprintf("rmnet_recv() smd lied about avail?!");
			p->rx_dropped++;
		} else {
			char pktbuf[1514];
			if (smd_read(p->ch, pktbuf, sz) != sz) {
				cprintf("rmnet_recv() smd lied about avail?!");
			} else {
				p->rx_frames++;
				p->rx_frame_bytes += sz;	
				// XXX- do shit!
			}

			printf("RECEIVED FRAME (%d bytes):", sz);
			hexdump((const unsigned char *)pktbuf, sz);
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

	if (smd_write(p->ch, buf, len) != len) {
		cprintf("rmnet fifo full, dropping packet\n");
	} else {
		p->tx_frames++;
		p->tx_frame_bytes += len;	
	}

	return 0;
}
