/* arch/arm/mach-msm/smd_tty.c
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
#include "smd_tty.h"

#define MAX_SMD_TTYS 32

static pthread_mutex_t smd_tty_lock;
static pthread_cond_t  smd_tty_cond;

struct smd_tty_info {
	smd_channel_t *ch;
	struct tty_struct *tty;
	int open_count;
	unsigned char rcvbuf[256];
	int   rcvbuf_read;
	int   rcvbuf_write;
};

static struct smd_tty_info smd_tty[MAX_SMD_TTYS];

static void smd_tty_notify(void *priv, unsigned event)
{
	unsigned char *ptr;
	int avail, i;
	struct smd_tty_info *info = (struct smd_tty_info *)priv;

	if (event != SMD_EVENT_DATA)
		return;

	pthread_mutex_lock(&smd_tty_lock);
	for (;;) {
		avail = smd_read_avail(info->ch);
		if (avail == 0) break;

		ptr = (unsigned char *)malloc(avail);
		if (ptr == NULL) {
			fprintf(stderr, "%s: malloc failed\n", __func__);
			exit(1);
		}

		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			cprintf("OOPS - smd_tty_buffer mismatch?!");
		}

		for (i = 0; i < avail; i++) {
			info->rcvbuf[info->rcvbuf_write] = ptr[i];
			info->rcvbuf_write = (info->rcvbuf_write + 1) %
			    sizeof(info->rcvbuf);
		}

		free(ptr);
	}
	pthread_cond_signal(&smd_tty_cond);
	pthread_mutex_unlock(&smd_tty_lock);
}

int smd_tty_open(int n)
{
	int res = 0;
	struct smd_tty_info *info;
	const char *name;

	if (n == 0) {
		name = "SMD_DS";
	} else if (n == 27) {
		name = "SMD_GPSNMEA";
	} else {
		return -E_NOT_FOUND;
	}

	info = smd_tty + n;

	pthread_mutex_lock(&smd_tty_lock);
	if (info->open_count++ == 0) {
		if (info->ch) {
			pthread_mutex_unlock(&smd_tty_lock);
			smd_kick(info->ch);
		} else {
			pthread_mutex_unlock(&smd_tty_lock);
			res = smd_open(name, &info->ch, info, smd_tty_notify);
		}
	}

	return res;
}

void smd_tty_close(int n)
{
	if (n != 0 && n != 27) {
		fprintf(stderr, "%s: invalid n (%d)\n", __func__, n);
		exit(1);
	}

	struct smd_tty_info *info = smd_tty + n;

	pthread_mutex_lock(&smd_tty_lock);
	if (--info->open_count == 0) {
		if (info->ch) {
			smd_close(info->ch);
			info->ch = 0;
			info->rcvbuf_read = info->rcvbuf_write = 0;
		}
	}
	pthread_mutex_unlock(&smd_tty_lock);
}

int smd_tty_read(int n, unsigned char *buf, int len)
{
	int i;

	if (n != 0 && n != 27) {
		fprintf(stderr, "%s: invalid n (%d)\n", __func__, n);
		exit(1);
	}

	struct smd_tty_info *info = smd_tty + n;

	pthread_mutex_lock(&smd_tty_lock);
	while (info->rcvbuf_read == info->rcvbuf_write)
		pthread_cond_wait(&smd_tty_cond, &smd_tty_lock);
	for (i = 0; i < len && info->rcvbuf_read != info->rcvbuf_write; i++) {
		buf[i] = info->rcvbuf[info->rcvbuf_read];
		info->rcvbuf_read = (info->rcvbuf_read + 1) %
		    sizeof(info->rcvbuf);
	}
	pthread_mutex_unlock(&smd_tty_lock);

	return ((i < len) ? i : len);
}

int smd_tty_write(int n, const unsigned char *buf, int len)
{
	if (n != 0 && n != 27) {
		fprintf(stderr, "%s: invalid n (%d)\n", __func__, n);
		exit(1);
	}

	struct smd_tty_info *info = smd_tty + n;
	int avail;

	/* if we're writing to a packet channel we will
	** never be able to write more data than there
	** is currently space for
	*/
	avail = smd_write_avail(info->ch);
	if (len > avail) {
		fprintf(stderr, "%s: dropping %d bytes\n", len - avail);
		len = avail;
	}

	return (smd_write(info->ch, buf, len));
}

int smd_tty_init(void)
{
	pthread_mutex_init(&smd_tty_lock, NULL);
	pthread_cond_init(&smd_tty_cond, NULL);

	return 0;
}
