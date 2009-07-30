/* arch/arm/mach-msm/smd_qmi.c
 *
 * QMI Control Driver -- Manages network data connections.
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

// The T-Mobile G1 does "up:epc.tmobile.com none none" to connect

enum { qmi_debug = 1 };

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

#define QMI_CTL 0x00
#define QMI_WDS 0x01
#define QMI_DMS 0x02
#define QMI_NAS 0x03

#define QMI_RESULT_SUCCESS 0x0000
#define QMI_RESULT_FAILURE 0x0001

struct qmi_msg {
	unsigned char service;
	unsigned char client_id;
	unsigned short txn_id;
	unsigned short type;
	unsigned short size;
	unsigned char *tlv;
};

#define qmi_ctl_client_id 0

#define STATE_OFFLINE    0
#define STATE_QUERYING   1
#define STATE_ONLINE     2

struct qmi_ctxt {
	pthread_mutex_t lock;

	unsigned char ctl_txn_id;
	unsigned char wds_client_id;
	unsigned short wds_txn_id;

	unsigned wds_busy;
	unsigned wds_handle;
	unsigned state_dirty;
	unsigned state;

	unsigned char addr[4];
	unsigned char mask[4];
	unsigned char gateway[4];
	unsigned char dns1[4];
	unsigned char dns2[4];

	smd_channel_t *ch;
	const char *ch_name;
};

static struct qmi_ctxt *qmi_minor_to_ctxt(unsigned n);

static struct qmi_ctxt qmi_device0, qmi_device1, qmi_device2;

static int verbose = 1;

static pthread_cond_t  qmi_waitq_cond;
static pthread_mutex_t qmi_waitq_mutex;

// XXX wrong, i know...
#define scnprintf snprintf

static void qmi_ctxt_init(struct qmi_ctxt *ctxt, const char *name)
{
	pthread_mutexattr_t lock_attr;
	pthread_mutexattr_init(&lock_attr);
	pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&ctxt->lock, &lock_attr);
	ctxt->ch_name = name;
	ctxt->ctl_txn_id = 1;
	ctxt->wds_txn_id = 1;
	ctxt->wds_busy = 1;
	ctxt->state = STATE_OFFLINE;
}

static void qmi_dump_msg(struct qmi_msg *msg, const char *prefix)
{
	unsigned sz, n;
	unsigned char *x;

	if (!verbose)
		return;

	cprintf("qmi: %s: svc=%02x cid=%02x tid=%04x type=%04x size=%04x\n",
	       prefix, msg->service, msg->client_id,
	       msg->txn_id, msg->type, msg->size);

	x = msg->tlv;
	sz = msg->size;

	while (sz >= 3) {
		sz -= 3;

		n = x[1] | (x[2] << 8);
		if (n > sz)
			break;

		cprintf("qmi: %s: tlv: %02x %04x { ",
		       prefix, x[0], n);
		x += 3;
		sz -= n;
		while (n-- > 0)
			cprintf("%02x ", *x++);
		cprintf("}\n");
	}
}

int qmi_add_tlv(struct qmi_msg *msg,
		unsigned type, unsigned size, const void *data)
{
	unsigned char *x = msg->tlv + msg->size;

	x[0] = type;
	x[1] = size;
	x[2] = size >> 8;

	memcpy(x + 3, data, size);

	msg->size += (size + 3);

	return 0;
}

/* Extract a tagged item from a qmi message buffer,
** taking care not to overrun the buffer.
*/
static int qmi_get_tlv(struct qmi_msg *msg,
		       unsigned type, unsigned size, void *data)
{
	unsigned char *x = msg->tlv;
	unsigned len = msg->size;
	unsigned n;

	while (len >= 3) {
		len -= 3;

		/* size of this item */
		n = x[1] | (x[2] << 8);
		if (n > len)
			break;

		if (x[0] == type) {
			if (n != size)
				return -1;
			memcpy(data, x + 3, size);
			return 0;
		}

		x += (n + 3);
		len -= n;
	}

	return -1;
}

static unsigned qmi_get_status(struct qmi_msg *msg, unsigned *error)
{
	unsigned short status[2];
	if (qmi_get_tlv(msg, 0x02, sizeof(status), status)) {
		*error = 0;
		return QMI_RESULT_FAILURE;
	} else {
		*error = status[1];
		return status[0];
	}
}

/* 0x01 <qmux-header> <payload> */
#define QMUX_HEADER    13

/* should be >= HEADER + FOOTER */
#define QMUX_OVERHEAD  16

static int qmi_send(struct qmi_ctxt *ctxt, struct qmi_msg *msg)
{
	unsigned char *data;
	unsigned hlen;
	unsigned len;
	int r;

	qmi_dump_msg(msg, "send");

	if (msg->service == QMI_CTL) {
		hlen = QMUX_HEADER - 1;
	} else {
		hlen = QMUX_HEADER;
	}

	/* QMUX length is total header + total payload - IFC selector */
	len = hlen + msg->size - 1;
	if (len > 0xffff)
		return -1;

	data = msg->tlv - hlen;

	/* prepend encap and qmux header */
	*data++ = 0x01; /* ifc selector */

	/* qmux header */
	*data++ = len;
	*data++ = len >> 8;
	*data++ = 0x00; /* flags: client */
	*data++ = msg->service;
	*data++ = msg->client_id;

	/* qmi header */
	*data++ = 0x00; /* flags: send */
	*data++ = msg->txn_id;
	if (msg->service != QMI_CTL)
		*data++ = msg->txn_id >> 8;

	*data++ = msg->type;
	*data++ = msg->type >> 8;
	*data++ = msg->size;
	*data++ = msg->size >> 8;

	/* len + 1 takes the interface selector into account */
	r = smd_write(ctxt->ch, msg->tlv - hlen, len + 1);

	if (r != (int)len) {
		return -1;
	} else {
		return 0;
	}
}

static void qmi_process_ctl_msg(struct qmi_ctxt *ctxt, struct qmi_msg *msg)
{
	unsigned err;

	if (qmi_debug)
		cprintf("%s: msg->type = 0x%x\n", __func__, msg->type);

	if (msg->type == 0x0022) {
		unsigned char n[2];
		if (qmi_get_status(msg, &err))
			return;
		if (qmi_get_tlv(msg, 0x01, sizeof(n), n))
			return;
		if (n[0] == QMI_WDS) {
			cprintf("qmi: ctl: wds use client_id 0x%02x\n", n[1]);
			ctxt->wds_client_id = n[1];
			ctxt->wds_busy = 0;
		}
	}
}

static int qmi_network_get_profile(struct qmi_ctxt *ctxt);

static void swapaddr(unsigned char *src, unsigned char *dst)
{
	dst[0] = src[3];
	dst[1] = src[2];
	dst[2] = src[1];
	dst[3] = src[0];
}

static unsigned char zero[4];
static void qmi_read_runtime_profile(struct qmi_ctxt *ctxt, struct qmi_msg *msg)
{
	unsigned char tmp[4];
	unsigned r;

	r = qmi_get_tlv(msg, 0x1e, 4, tmp);
	swapaddr(r ? zero : tmp, ctxt->addr);
	r = qmi_get_tlv(msg, 0x21, 4, tmp);
	swapaddr(r ? zero : tmp, ctxt->mask);
	r = qmi_get_tlv(msg, 0x20, 4, tmp);
	swapaddr(r ? zero : tmp, ctxt->gateway);
	r = qmi_get_tlv(msg, 0x15, 4, tmp);
	swapaddr(r ? zero : tmp, ctxt->dns1);
	r = qmi_get_tlv(msg, 0x16, 4, tmp);
	swapaddr(r ? zero : tmp, ctxt->dns2);
}

static void qmi_process_unicast_wds_msg(struct qmi_ctxt *ctxt,
					struct qmi_msg *msg)
{
	if (qmi_debug)
		cprintf("%s: ctxt %p\n", __func__, ctxt);

	unsigned err;
	switch (msg->type) {
	case 0x0021:
		if (qmi_get_status(msg, &err)) {
			cprintf("qmi: wds: network stop failed (%04x)\n", err);
		} else {
			cprintf("qmi: wds: network stopped\n");
			ctxt->state = STATE_OFFLINE;
			if (qmi_debug)
				cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
			ctxt->state_dirty = 1;
		}
		break;
	case 0x0020:
		if (qmi_get_status(msg, &err)) {
			cprintf("qmi: wds: network start failed (%04x)\n", err);
		} else if (qmi_get_tlv(msg, 0x01, sizeof(ctxt->wds_handle), &ctxt->wds_handle)) {
			cprintf("qmi: wds no handle?\n");
		} else {
			cprintf("qmi: wds: got handle 0x%08x\n",
			       ctxt->wds_handle);
		}
		break;
	case 0x002D:
		cprintf("qmi: got network profile\n");
		if (ctxt->state == STATE_QUERYING) {
			qmi_read_runtime_profile(ctxt, msg);
			ctxt->state = STATE_ONLINE;
			if (qmi_debug)
				cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
			ctxt->state_dirty = 1;
		}
		break;
	default:
		cprintf("qmi: unknown msg type 0x%04x\n", msg->type);
	}
	ctxt->wds_busy = 0;
}

static void qmi_process_broadcast_wds_msg(struct qmi_ctxt *ctxt,
					  struct qmi_msg *msg)
{
	if (qmi_debug)
		cprintf("%s: ctxt %p\n", __func__, ctxt);

	if (msg->type == 0x0022) {
		unsigned char n[2];
		if (qmi_get_tlv(msg, 0x01, sizeof(n), n))
			return;
		switch (n[0]) {
		case 1:
			cprintf("qmi: wds: DISCONNECTED\n");
			ctxt->state = STATE_OFFLINE;
			if (qmi_debug)
				cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
			ctxt->state_dirty = 1;
			break;
		case 2:
			cprintf("qmi: wds: CONNECTED\n");
			ctxt->state = STATE_QUERYING;
			if (qmi_debug)
				cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
			ctxt->state_dirty = 1;
			qmi_network_get_profile(ctxt);
			break;
		case 3:
			cprintf("qmi: wds: SUSPENDED\n");
			ctxt->state = STATE_OFFLINE;
			if (qmi_debug)
				cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
			ctxt->state_dirty = 1;
		default:
			cprintf("qmi: wds: WHAT THE FUCK!? %d\n", n[0]);
		}
	} else {
		cprintf("qmi: unknown bcast msg type 0x%04x\n", msg->type);
	}
}

static void qmi_process_wds_msg(struct qmi_ctxt *ctxt,
				struct qmi_msg *msg)
{
	cprintf("wds: %04x @ %02x\n", msg->type, msg->client_id);
	if (msg->client_id == ctxt->wds_client_id) {
		qmi_process_unicast_wds_msg(ctxt, msg);
	} else if (msg->client_id == 0xff) {
		qmi_process_broadcast_wds_msg(ctxt, msg);
	} else {
		cprintf("qmi_process_wds_msg client id 0x%02x unknown\n",
		       msg->client_id);
	}
}

static void qmi_process_qmux(struct qmi_ctxt *ctxt,
			     unsigned char *buf, unsigned sz)
{
	struct qmi_msg msg;

	if (qmi_debug)
		cprintf("%s: working on ctxt %p\n", __func__, ctxt);

	/* require a full header */
	if (sz < 5) {
		cprintf("%s: sz < 5 (sz == %u)\n", __func__, sz);
		return;
	}

	/* require a size that matches the buffer size */
	if ((int)sz != (buf[0] | (buf[1] << 8))) {
		cprintf("%s: sz doesn't match buffer size\n", __func__);
		return;
	}

	/* only messages from a service (bit7=1) are allowed */
	if (buf[2] != 0x80) {
		cprintf("%s: buf[2] != 0x80\n", __func__);
		return;
	}

	msg.service = buf[3];
	msg.client_id = buf[4];

	/* annoyingly, CTL messages have a shorter TID */
	if (buf[3] == 0) {
		if (sz < 7)
			return;
		msg.txn_id = buf[6];
		buf += 7;
		sz -= 7;
	} else {
		if (sz < 8)
			return;
		msg.txn_id = buf[6] | (buf[7] << 8);
		buf += 8;
		sz -= 8;
	}

	/* no type and size!? */
	if (sz < 4) {
		cprintf("%s: no type and no size\n", __func__);
		return;
	}
	sz -= 4;

	msg.type = buf[0] | (buf[1] << 8);
	msg.size = buf[2] | (buf[3] << 8);
	msg.tlv = buf + 4;

	if (sz != msg.size) {
		cprintf("%s: sz != msg.size\n", __func__);
		return;
	}

	qmi_dump_msg(&msg, "recv");

	pthread_mutex_lock(&ctxt->lock);
	switch (msg.service) {
	case QMI_CTL:
		qmi_process_ctl_msg(ctxt, &msg);
		break;
	case QMI_WDS:
		qmi_process_wds_msg(ctxt, &msg);
		break;
	default:
		cprintf("qmi: msg from unknown svc 0x%02x\n",
		       msg.service);
		break;
	}
	pthread_mutex_unlock(&ctxt->lock);

	pthread_mutex_lock(&qmi_waitq_mutex);
	pthread_cond_broadcast(&qmi_waitq_cond);
	pthread_mutex_unlock(&qmi_waitq_mutex);

	if (qmi_debug)
		cprintf("%s: tickled wait queue\n", __func__); 
}

#define QMI_MAX_PACKET (256 + QMUX_OVERHEAD)

static void qmi_read_work(struct qmi_ctxt *ctxt)
{
	struct smd_channel *ch = ctxt->ch;
	unsigned char buf[QMI_MAX_PACKET];
	int sz;

	if (qmi_debug)
		cprintf("%s: processing for ctxt %p\n", __func__, ctxt);

	for (;;) {
		sz = smd_cur_packet_size(ch);
		if (sz == 0)
			break;
		if (sz < smd_read_avail(ch))
			break;
		if (sz > QMI_MAX_PACKET) {
			smd_read(ch, 0, sz);
			continue;
		}
		if (smd_read(ch, buf, sz) != sz) {
			cprintf("qmi: not enough data?!\n");
			continue;
		}

		/* interface selector must be 1 */
		if (buf[0] != 0x01)
			continue;

		qmi_process_qmux(ctxt, buf + 1, sz - 1);
	}
}

static int qmi_request_wds_cid(struct qmi_ctxt *ctxt);

static void qmi_open_work(struct qmi_ctxt *ctxt)
{
	pthread_mutex_lock(&ctxt->lock);
	qmi_request_wds_cid(ctxt);
	pthread_mutex_unlock(&ctxt->lock);
}

static void qmi_notify(void *priv, unsigned event)
{
	struct qmi_ctxt *ctxt = (struct qmi_ctxt *)priv;

	if (qmi_debug)
		cprintf("%s: entered on event %u\n", __func__, event);
	
	switch (event) {
	case SMD_EVENT_DATA: {
		int sz;
		sz = smd_cur_packet_size(ctxt->ch);
		if ((sz > 0) && (sz <= smd_read_avail(ctxt->ch)))
			qmi_read_work(ctxt);
		else if (qmi_debug)
			cprintf("%s: SMD_EVENT_DATA: sz = %d, read_avail = %u\n", __func__, sz, smd_read_avail(ctxt->ch)); 
		break;
	}
	case SMD_EVENT_OPEN:
		cprintf("qmi: smd opened\n");
		qmi_open_work(ctxt);
		break;
	case SMD_EVENT_CLOSE:
		cprintf("qmi: smd closed\n");
		break;

	default:
		cprintf("qmi: notify got weird event %u\n", event);
	}
}

static int qmi_request_wds_cid(struct qmi_ctxt *ctxt)
{
	unsigned char data[64 + QMUX_OVERHEAD];
	struct qmi_msg msg;
	unsigned char n;

	msg.service = QMI_CTL;
	msg.client_id = qmi_ctl_client_id;
	msg.txn_id = ctxt->ctl_txn_id;
	msg.type = 0x0022;
	msg.size = 0;
	msg.tlv = data + QMUX_HEADER;

	ctxt->ctl_txn_id += 2;

	n = QMI_WDS;
	qmi_add_tlv(&msg, 0x01, 0x01, &n);

	return qmi_send(ctxt, &msg);
}

static int qmi_network_get_profile(struct qmi_ctxt *ctxt)
{
	unsigned char data[96 + QMUX_OVERHEAD];
	struct qmi_msg msg;

	msg.service = QMI_WDS;
	msg.client_id = ctxt->wds_client_id;
	msg.txn_id = ctxt->wds_txn_id;
	msg.type = 0x002D;
	msg.size = 0;
	msg.tlv = data + QMUX_HEADER;

	ctxt->wds_txn_id += 2;

	return qmi_send(ctxt, &msg);
}

static int qmi_network_up(struct qmi_ctxt *ctxt, char *apn)
{
	unsigned char data[96 + QMUX_OVERHEAD];
	struct qmi_msg msg;
	char *user;
	char *pass;

	for (user = apn; *user; user++) {
		if (*user == ' ') {
			*user++ = 0;
			break;
		}
	}
	for (pass = user; *pass; pass++) {
		if (*pass == ' ') {
			*pass++ = 0;
			break;
		}
	}

	msg.service = QMI_WDS;
	msg.client_id = ctxt->wds_client_id;
	msg.txn_id = ctxt->wds_txn_id;
	msg.type = 0x0020;
	msg.size = 0;
	msg.tlv = data + QMUX_HEADER;

	ctxt->wds_txn_id += 2;

	qmi_add_tlv(&msg, 0x14, strlen(apn), apn);
	if (*user) {
		unsigned char x;
		x = 3;
		qmi_add_tlv(&msg, 0x16, 1, &x);
		qmi_add_tlv(&msg, 0x17, strlen(user), user);
		if (*pass)
			qmi_add_tlv(&msg, 0x18, strlen(pass), pass);
	}
	return qmi_send(ctxt, &msg);
}

static int qmi_network_down(struct qmi_ctxt *ctxt)
{
	unsigned char data[16 + QMUX_OVERHEAD];
	struct qmi_msg msg;

	msg.service = QMI_WDS;
	msg.client_id = ctxt->wds_client_id;
	msg.txn_id = ctxt->wds_txn_id;
	msg.type = 0x0021;
	msg.size = 0;
	msg.tlv = data + QMUX_HEADER;

	ctxt->wds_txn_id += 2;

	qmi_add_tlv(&msg, 0x01, sizeof(ctxt->wds_handle), &ctxt->wds_handle);

	return qmi_send(ctxt, &msg);
}

static int qmi_print_state(struct qmi_ctxt *ctxt, char *buf, int max)
{
	int i;
	const char *statename;

	if (ctxt->state == STATE_ONLINE) {
		statename = "up";
	} else if (ctxt->state == STATE_OFFLINE) {
		statename = "down";
	} else {
		statename = "busy";
	}

	i = scnprintf(buf, max, "STATE=%s\n", statename);
	i += scnprintf(buf + i, max - i, "CID=%d\n",ctxt->wds_client_id);

	if (ctxt->state != STATE_ONLINE){
		return i;
	}

	i += scnprintf(buf + i, max - i, "ADDR=%d.%d.%d.%d\n",
		ctxt->addr[0], ctxt->addr[1], ctxt->addr[2], ctxt->addr[3]);
	i += scnprintf(buf + i, max - i, "MASK=%d.%d.%d.%d\n",
		ctxt->mask[0], ctxt->mask[1], ctxt->mask[2], ctxt->mask[3]);
	i += scnprintf(buf + i, max - i, "GATEWAY=%d.%d.%d.%d\n",
		ctxt->gateway[0], ctxt->gateway[1], ctxt->gateway[2],
		ctxt->gateway[3]);
	i += scnprintf(buf + i, max - i, "DNS1=%d.%d.%d.%d\n",
		ctxt->dns1[0], ctxt->dns1[1], ctxt->dns1[2], ctxt->dns1[3]);
	i += scnprintf(buf + i, max - i, "DNS2=%d.%d.%d.%d\n",
		ctxt->dns2[0], ctxt->dns2[1], ctxt->dns2[2], ctxt->dns2[3]);

	return i;
}

extern "C" int smd_qmi_read(int n, unsigned char *buf, int count)
{
	struct qmi_ctxt *ctxt = qmi_minor_to_ctxt(n);
	char msg[256];
	int len;

	if (qmi_debug)
		cprintf("%s: ctxt %p (%d), count = %d\n", __func__, ctxt, n, count);

	pthread_mutex_lock(&ctxt->lock);
	for (;;) {
		if (ctxt->state_dirty) {
			ctxt->state_dirty = 0;
			len = qmi_print_state(ctxt, msg, 256);
			break;
		}
		pthread_mutex_unlock(&ctxt->lock);

		pthread_mutex_lock(&qmi_waitq_mutex);
		if (!ctxt->state_dirty)
			pthread_cond_wait(&qmi_waitq_cond, &qmi_waitq_mutex);
		pthread_mutex_unlock(&qmi_waitq_mutex);

		pthread_mutex_lock(&ctxt->lock);
	}
	pthread_mutex_unlock(&ctxt->lock);

	if (len > count)
		len = count;

	memcpy(buf, msg, len);

	return len;
}

extern "C" void smd_qmi_readwait(const int *ns, int *rdys, const int num)
{
	int i, anyrdy = 0;

	memset(rdys, 0, sizeof(rdys[0]) * num);

	if (qmi_debug) {
		cprintf("%s: %d waiters:\n", __func__, num);
		for (i = 0; i < num; i++)
			cprintf("  %d: %d\n", i, ns[i]);
	}

	pthread_mutex_lock(&qmi_waitq_mutex);
	for (;;) {
		for (i = 0; i < num; i++) {
			struct qmi_ctxt *ctxt = qmi_minor_to_ctxt(ns[i]);
			pthread_mutex_lock(&ctxt->lock);
			if (ctxt->state_dirty) {
				rdys[i] = 1;
				anyrdy = 1;
			}
			pthread_mutex_unlock(&ctxt->lock);
		}

		if (anyrdy) {
			pthread_mutex_unlock(&qmi_waitq_mutex);
			if (qmi_debug)
				cprintf("%s: returning on ready!\n", __func__);
			return;
		}

		pthread_cond_wait(&qmi_waitq_cond, &qmi_waitq_mutex);
	}
}

extern "C" int smd_qmi_write(int n, const unsigned char *buf, int count)
{
	struct qmi_ctxt *ctxt = qmi_minor_to_ctxt(n);
	unsigned char cmd[64];
	int len;

	if (qmi_debug)
		cprintf("%s: ctxt %p (%d), count = %d\n", __func__, ctxt, n, count);

	if (count < 1)
		return 0;

	len = count > 63 ? 63 : count;

	memcpy(cmd, buf, len);

	cmd[len] = 0;

	/* lazy */
	if (cmd[len-1] == '\n') {
		cmd[len-1] = 0;
		len--;
	}

	if (!strncmp((const char *)cmd, "verbose", 7)) {
		verbose = 1;
	} else if (!strncmp((const char *)cmd, "terse", 5)) {
		verbose = 0;
	} else if (!strncmp((const char *)cmd, "poll", 4)) {
		if (qmi_debug)
			cprintf("%s(%s.%d): state_dirty\n", __func__, __FILE__, __LINE__);
		ctxt->state_dirty = 1;
		pthread_mutex_lock(&qmi_waitq_mutex);
		pthread_cond_broadcast(&qmi_waitq_cond);
		pthread_mutex_unlock(&qmi_waitq_mutex);
	} else if (!strncmp((const char *)cmd, "down", 4)) {
retry_down:
		pthread_mutex_lock(&ctxt->lock);
		if (ctxt->wds_busy) {
			pthread_mutex_unlock(&ctxt->lock);

			pthread_mutex_lock(&qmi_waitq_mutex);
			if (ctxt->wds_busy)
				pthread_cond_wait(&qmi_waitq_cond,
				    &qmi_waitq_mutex);
			pthread_mutex_unlock(&qmi_waitq_mutex);

			goto retry_down;
		}
		ctxt->wds_busy = 1;
		qmi_network_down(ctxt);
		pthread_mutex_unlock(&ctxt->lock);
	} else if (!strncmp((const char *)cmd, "up:", 3)) {
retry_up:
		pthread_mutex_lock(&ctxt->lock);
		if (ctxt->wds_busy) {
			pthread_mutex_unlock(&ctxt->lock);

			pthread_mutex_lock(&qmi_waitq_mutex);
			if (ctxt->wds_busy)
				pthread_cond_wait(&qmi_waitq_cond,
				    &qmi_waitq_mutex);
			pthread_mutex_unlock(&qmi_waitq_mutex);

			goto retry_up;
		}
		ctxt->wds_busy = 1;
		qmi_network_up(ctxt, (char *)cmd+3);
		pthread_mutex_unlock(&ctxt->lock);
	} else {
		return -E_INVAL;
	}

	return count;
}

extern "C" int smd_qmi_open(int n)
{
	struct qmi_ctxt *ctxt = qmi_minor_to_ctxt(n);
	int r = 0;

	if (!ctxt) {
		cprintf("unknown qmi misc %d\n", n);
		return -E_NOT_FOUND;
	}

	pthread_mutex_lock(&ctxt->lock);
	if (ctxt->ch == 0)
		r = smd_open(ctxt->ch_name, &ctxt->ch, ctxt, qmi_notify);
	if (r == 0) {
		pthread_mutex_lock(&qmi_waitq_mutex);
		pthread_cond_broadcast(&qmi_waitq_cond);
		pthread_mutex_unlock(&qmi_waitq_mutex);
	}
	pthread_mutex_unlock(&ctxt->lock);

	return r;
}

extern "C" void smd_qmi_close(int n)
{
}

static struct qmi_ctxt *qmi_minor_to_ctxt(unsigned n)
{
	if (n == 0)
		return &qmi_device0;
	if (n == 1)
		return &qmi_device1;
	if (n == 2)
		return &qmi_device2;

	cprintf("%s: invalid index %d\n", __func__, n);
	exit(1);
}

extern "C" int smd_qmi_init(void)
{
	qmi_ctxt_init(&qmi_device0, "SMD_DATA5_CNTL");
	qmi_ctxt_init(&qmi_device1, "SMD_DATA6_CNTL");
	qmi_ctxt_init(&qmi_device2, "SMD_DATA7_CNTL");

	pthread_mutex_init(&qmi_waitq_mutex, NULL);
	pthread_cond_init(&qmi_waitq_cond, NULL);

	return 0;
}
