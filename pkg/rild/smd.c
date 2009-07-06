/* arch/arm/mach-msm/smd.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/mman.h>

#include <inc/assert.h>
#include <inc/atomic.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/queue.h>
#include <inc/syscall.h>

#include "msm_smd.h"
#include "smd_private.h"

enum {
	MSM_SMD_DEBUG = 1U << 0,
	MSM_SMSM_DEBUG = 1U << 1,
};
static int msm_smd_debug_mask = 0; //(MSM_SMD_DEBUG | MSM_SMSM_DEBUG);

volatile void *smem_find(unsigned id, unsigned size);
void smd_diag(void);

static unsigned last_heap_free = 0xffffffff;

#if 1
#define D(x...) cprintf(x)
#else
#define D(x...) do {} while (0)
#endif

static volatile void *notify_segment;
static const uint32_t notify_segment_len = 4096;
static const char    *notify_segment_device = "/dev/fb2";

static volatile void *smd_segment;
static const uint32_t smd_segment_len = 1024 * 1024;
static const char    *smd_segment_device = "/dev/fb1";

#define MSM_SHARED_RAM_BASE	smd_segment

#define MSM_A2M_INT(n) \
    ((volatile uint32_t *)((volatile char *)notify_segment + 0x400 + (n) * 4))

#define INT_A9_M2A_0	0
#define INT_A9_M2A_5	5

typedef int irqreturn_t;
#define IRQ_HANDLED	1

#define ALIGN(_x, _y)	(((_x) + ((_y) - 1)) & ~((_y) - 1))

static jos_atomic_t irq_thread_semaphore = { 2 };

static inline void notify_other_smsm(void)
{
	*MSM_A2M_INT(5) = 1;
}

static inline void notify_other_smd(void)
{
	*MSM_A2M_INT(0) = 1;
}

void smd_diag(void)
{
	volatile char *x;

	x = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
	if (x != 0) {
		x[SZ_DIAG_ERR_MSG - 1] = 0;
		cprintf("smem: DIAG '%s'\n", x);
	}
}

/* call when SMSM_RESET flag is set in the A9's smsm_state */
static void handle_modem_crash(void)
{
	cprintf("ARM9 has CRASHED\n");
	smd_diag();

	/* in this case the modem or watchdog should reboot us */
	for (;;) ;
}

#if 0
extern int (*msm_check_for_modem_crash)(void);

static int check_for_modem_crash(void)
{
	volatile struct smsm_shared *smsm;

	smsm = smem_find(ID_SHARED_STATE, 2 * sizeof(struct smsm_shared));

	/* if the modem's not ready yet, we have to hope for the best */
	if (!smsm)
		return 0;

	if (smsm[1].state & SMSM_RESET) {
		handle_modem_crash();
		return -1;
	} else {
		return 0;
	}
}
#endif

#define SMD_SS_CLOSED            0x00000000
#define SMD_SS_OPENING           0x00000001
#define SMD_SS_OPENED            0x00000002
#define SMD_SS_FLUSHING          0x00000003
#define SMD_SS_CLOSING           0x00000004
#define SMD_SS_RESET             0x00000005
#define SMD_SS_RESET_OPENING     0x00000006

#define SMD_BUF_SIZE 8192
#define SMD_CHANNELS 64

#define SMD_HEADER_SIZE 20


/* the mutex is used to synchronize between the
** irq handler and code that mutates the channel
** list or fiddles with channel state
*/
static pthread_mutex_t smd_mutex;
static pthread_mutex_t smem_mutex;

/* the mutex is used during open() and close()
** operations to avoid races while creating or
** destroying smd_channel structures
*/
static pthread_mutex_t smd_creation_mutex;

// don't let both irq threads run simultaneously
static pthread_mutex_t irq_mutex;

static int smd_initialized = 0;

struct smd_alloc_elm {
	char name[20];
	uint32_t cid;
	uint32_t ctype;
	uint32_t ref_count;
};
	
struct smd_half_channel
{
	unsigned state;
	unsigned char fDSR;
	unsigned char fCTS;
	unsigned char fCD;
	unsigned char fRI;
	unsigned char fHEAD;
	unsigned char fTAIL;
	unsigned char fSTATE;
	unsigned char fUNUSED;
	unsigned tail;
	unsigned head;
	unsigned char data[SMD_BUF_SIZE];
};

struct smd_shared
{
	struct smd_half_channel ch0;
	struct smd_half_channel ch1;
};

struct smd_channel
{
	volatile struct smd_half_channel *send;
	volatile struct smd_half_channel *recv;
	LIST_ENTRY(smd_channel) ch_list;

	unsigned current_packet;
	unsigned n;
	void *priv;
	void (*notify)(void *priv, unsigned flags);

	int (*read)(smd_channel_t *ch, void *data, int len);
	int (*write)(smd_channel_t *ch, const void *data, int len);
	int (*read_avail)(smd_channel_t *ch);
	int (*write_avail)(smd_channel_t *ch);

	void (*update_state)(smd_channel_t *ch);
	unsigned last_state;

	char name[32];
};

static LIST_HEAD(, smd_channel) smd_ch_closed_list = LIST_HEAD_INITIALIZER(head);
static LIST_HEAD(, smd_channel) smd_ch_list = LIST_HEAD_INITIALIZER(head);

static unsigned char smd_ch_allocated[64];

static void smd_alloc_channel(const char *name, uint32_t cid, uint32_t type);

static void smd_channel_probe()
{
	volatile struct smd_alloc_elm *shared;
	unsigned n;

	D("%s: running\n", __func__);

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * 64);

	for (n = 0; n < 64; n++) {
		if (smd_ch_allocated[n])
			continue;
		if (!shared[n].ref_count)
			continue;
		if (!shared[n].name[0])
			continue;
		smd_alloc_channel((const char *)shared[n].name, 
				  shared[n].cid,
				  shared[n].ctype);
		smd_ch_allocated[n] = 1;
	}
}

static const char *chstate(unsigned n)
{
	switch (n) {
	case SMD_SS_CLOSED:        return "CLOSED";
	case SMD_SS_OPENING:       return "OPENING";
	case SMD_SS_OPENED:        return "OPENED";
	case SMD_SS_FLUSHING:      return "FLUSHING";
	case SMD_SS_CLOSING:       return "CLOSING";
	case SMD_SS_RESET:         return "RESET";
	case SMD_SS_RESET_OPENING: return "ROPENING";
	default:                   return "UNKNOWN";
	}
}

/* how many bytes are available for reading */
static int smd_stream_read_avail(struct smd_channel *ch)
{
	return (ch->recv->head - ch->recv->tail) & (SMD_BUF_SIZE - 1);
}

/* how many bytes we are free to write */
static int smd_stream_write_avail(struct smd_channel *ch)
{
	return (SMD_BUF_SIZE - 1) -
		((ch->send->head - ch->send->tail) & (SMD_BUF_SIZE - 1));
}

static int smd_packet_read_avail(struct smd_channel *ch)
{
	if (ch->current_packet) {
		unsigned int n = smd_stream_read_avail(ch);
		if (n > ch->current_packet)
			n = ch->current_packet;
		return n;
	} else {
		return 0;
	}
}

static int smd_packet_write_avail(struct smd_channel *ch)
{
	int n = smd_stream_write_avail(ch);
	return n > SMD_HEADER_SIZE ? n - SMD_HEADER_SIZE : 0;
}

static int ch_is_open(struct smd_channel *ch)
{
	return (ch->recv->state == SMD_SS_OPENED) &&
		(ch->send->state == SMD_SS_OPENED);
}

/* provide a pointer and length to readable data in the fifo */
static unsigned ch_read_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->recv->head;
	unsigned tail = ch->recv->tail;
	*ptr = (void *) (ch->recv->data + tail);

	if (tail <= head) {
		return head - tail;
	} else {
		return SMD_BUF_SIZE - tail;
	}
}

/* advance the fifo read pointer after data from ch_read_buffer is consumed */
static void ch_read_done(struct smd_channel *ch, unsigned count)
{
	assert((signed)count <= smd_stream_read_avail(ch));
	ch->recv->tail = (ch->recv->tail + count) & (SMD_BUF_SIZE - 1);
	ch->recv->fTAIL = 1;
}

/* basic read interface to ch_read_{buffer,done} used
** by smd_*_read() and update_packet_state()
** will read-and-discard if the _data pointer is null
*/
static int ch_read(struct smd_channel *ch, void *_data, int len)
{
	void *ptr;
	unsigned n;
	unsigned char *data = _data;
	int orig_len = len;

	while (len > 0) {
		n = ch_read_buffer(ch, &ptr);
		if (n == 0)
			break;

		if ((signed)n > len)
			n = len;
		if (_data)
			memcpy(data, ptr, n);

		data += n;
		len -= n;
		ch_read_done(ch, n);
	}

	return orig_len - len;
}

static void update_stream_state(struct smd_channel *ch)
{
	/* streams have no special state requiring updating */
}

static void update_packet_state(struct smd_channel *ch)
{
	unsigned hdr[5];
	int r;

	/* can't do anything if we're in the middle of a packet */
	if (ch->current_packet != 0) return;

	/* don't bother unless we can get the full header */
	if (smd_stream_read_avail(ch) < SMD_HEADER_SIZE) return;

	r = ch_read(ch, hdr, SMD_HEADER_SIZE);
	assert(r == SMD_HEADER_SIZE);

	ch->current_packet = hdr[0];
}

/* provide a pointer and length to next free space in the fifo */
static unsigned ch_write_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->send->head;
	unsigned tail = ch->send->tail;
	*ptr = (void *) (ch->send->data + head);

	if (head < tail) {
		return tail - head - 1;
	} else {
		if (tail == 0) {
			return SMD_BUF_SIZE - head - 1;
		} else {
			return SMD_BUF_SIZE - head;
		}
	}
}

/* advace the fifo write pointer after freespace from ch_write_buffer is filled */
static void ch_write_done(struct smd_channel *ch, unsigned count)
{
	assert((signed)count <= smd_stream_write_avail(ch));
	ch->send->head = (ch->send->head + count) & (SMD_BUF_SIZE - 1);
	ch->send->fHEAD = 1;
}

static void hc_set_state(volatile struct smd_half_channel *hc, unsigned n)
{
	if (n == SMD_SS_OPENED) {
		hc->fDSR = 1;
		hc->fCTS = 1;
		hc->fCD = 1;
	} else {
		hc->fDSR = 0;
		hc->fCTS = 0;
		hc->fCD = 0;
	}
	hc->state = n;
	hc->fSTATE = 1;
	notify_other_smd();
}

static void do_smd_probe(void)
{
	volatile struct smem_shared *shared = (volatile void *) MSM_SHARED_RAM_BASE;
	if (shared->heap_info.free_offset != last_heap_free) {
		last_heap_free = shared->heap_info.free_offset;
		smd_channel_probe();
	}
}

static void smd_state_change(struct smd_channel *ch,
			     unsigned last, unsigned next)
{
	ch->last_state = next;

	cprintf("SMD: ch %d %s -> %s\n", ch->n,
	       chstate(last), chstate(next));

	switch(next) {
	case SMD_SS_OPENING:
		ch->recv->tail = 0;
	case SMD_SS_OPENED:
		if (ch->send->state != SMD_SS_OPENED) {
			hc_set_state(ch->send, SMD_SS_OPENED);
		}
		ch->notify(ch->priv, SMD_EVENT_OPEN);
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
		/* we should force them to close? */
	default:
		ch->notify(ch->priv, SMD_EVENT_CLOSE);
	}
}

static irqreturn_t smd_irq_handler(int irq, void *data)
{
	struct smd_channel *ch;
	int do_notify = 0;
	unsigned ch_flags;
	unsigned tmp;
/*	D("<SMD>\n"); */

	pthread_mutex_lock(&smd_mutex);
	LIST_FOREACH(ch, &smd_ch_list, ch_list) {
		ch_flags = 0;
		if (ch_is_open(ch)) {
			if (ch->recv->fHEAD) {
				ch->recv->fHEAD = 0;
				ch_flags |= 1;
				do_notify |= 1;
			}
			if (ch->recv->fTAIL) {
				ch->recv->fTAIL = 0;
				ch_flags |= 2;
				do_notify |= 1;
			}
			if (ch->recv->fSTATE) {
				ch->recv->fSTATE = 0;
				ch_flags |= 4;
				do_notify |= 1;
			}
		}
		tmp = ch->recv->state;
		if (tmp != ch->last_state)
			smd_state_change(ch, ch->last_state, tmp);
		if (ch_flags) {
			ch->update_state(ch);
			ch->notify(ch->priv, SMD_EVENT_DATA);
		}
	}
	if (do_notify) notify_other_smd();
	pthread_mutex_unlock(&smd_mutex);
	do_smd_probe();
	return IRQ_HANDLED;
}

void smd_sleep_exit(void)
{
	struct smd_channel *ch;
	unsigned tmp;
	int need_int = 0;

	pthread_mutex_lock(&smd_mutex);
	LIST_FOREACH(ch, &smd_ch_list, ch_list) {
		if (ch_is_open(ch)) {
			if (ch->recv->fHEAD) {
				if (msm_smd_debug_mask & MSM_SMD_DEBUG)
					cprintf("smd_sleep_exit ch %d fHEAD "
						"%x %x %x\n",
						ch->n, ch->recv->fHEAD,
						ch->recv->head, ch->recv->tail);
				need_int = 1;
				break;
			}
			if (ch->recv->fTAIL) {
				if (msm_smd_debug_mask & MSM_SMD_DEBUG)
					cprintf("smd_sleep_exit ch %d fTAIL "
						"%x %x %x\n",
						ch->n, ch->recv->fTAIL,
						ch->send->head, ch->send->tail);
				need_int = 1;
				break;
			}
			if (ch->recv->fSTATE) {
				if (msm_smd_debug_mask & MSM_SMD_DEBUG)
					cprintf("smd_sleep_exit ch %d fSTATE %x"
						"\n", ch->n, ch->recv->fSTATE);
				need_int = 1;
				break;
			}
			tmp = ch->recv->state;
			if (tmp != ch->last_state) {
				if (msm_smd_debug_mask & MSM_SMD_DEBUG)
					cprintf("smd_sleep_exit ch %d "
						"state %x != %x\n",
						ch->n, tmp, ch->last_state);
				need_int = 1;
				break;
			}
		}
	}
	pthread_mutex_unlock(&smd_mutex);
	do_smd_probe();
	if (need_int) {
		if (msm_smd_debug_mask & MSM_SMD_DEBUG)
			cprintf("smd_sleep_exit need interrupt\n");
		smd_irq_handler(0, NULL);
	}
}


void smd_kick(smd_channel_t *ch)
{
	unsigned tmp;

	pthread_mutex_lock(&smd_mutex);
	ch->update_state(ch);
	tmp = ch->recv->state;
	if (tmp != ch->last_state) {
		ch->last_state = tmp;
		if (tmp == SMD_SS_OPENED) {
			ch->notify(ch->priv, SMD_EVENT_OPEN);
		} else {
			ch->notify(ch->priv, SMD_EVENT_CLOSE);
		}
	}
	ch->notify(ch->priv, SMD_EVENT_DATA);
	notify_other_smd();
	pthread_mutex_unlock(&smd_mutex);
}

static int smd_is_packet(int chn)
{
	if ((chn > 4) || (chn == 1)) {
		return 1;
	} else {
		return 0;
	}
}

static int smd_stream_write(smd_channel_t *ch, const void *_data, int len)
{
	void *ptr;
	const unsigned char *buf = _data;
	unsigned xfer;
	int orig_len = len;

	D("smd_stream_write() %d -> ch%d\n", len, ch->n);
	if (len < 0) return -E_INVAL;

	while ((xfer = ch_write_buffer(ch, &ptr)) != 0) {
		if (!ch_is_open(ch))
			break;
		if ((signed)xfer > len)
			xfer = len;
		memcpy(ptr, buf, xfer);
		ch_write_done(ch, xfer);
		len -= xfer;
		buf += xfer;
		if (len == 0)
			break;
	}

	notify_other_smd();

	return orig_len - len;
}

static int smd_packet_write(smd_channel_t *ch, const void *_data, int len)
{
	unsigned hdr[5];

	D("smd_packet_write() %d -> ch%d\n", len, ch->n);
	if (len < 0) return -E_INVAL;

	if (smd_stream_write_avail(ch) < (len + SMD_HEADER_SIZE))
		return -E_NO_MEM;

	hdr[0] = len;
	hdr[1] = hdr[2] = hdr[3] = hdr[4] = 0;

	smd_stream_write(ch, hdr, sizeof(hdr));
	smd_stream_write(ch, _data, len);

	return len;
}

static int smd_stream_read(smd_channel_t *ch, void *data, int len)
{
	int r;

	if (len < 0) return -E_INVAL;

	r = ch_read(ch, data, len);
	if (r > 0)
		notify_other_smd();

	return r;
}

static int smd_packet_read(smd_channel_t *ch, void *data, int len)
{
	int r;

	if (len < 0) return -E_INVAL;

	if (len > (signed)ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len);
	if (r > 0)
		notify_other_smd();

	pthread_mutex_lock(&smd_mutex);
	ch->current_packet -= r;
	update_packet_state(ch);
	pthread_mutex_unlock(&smd_mutex);

	return r;
}

static void smd_alloc_channel(const char *name, uint32_t cid, uint32_t type)
{
	struct smd_channel *ch;
	volatile struct smd_shared *shared;

	shared = smem_alloc(ID_SMD_CHANNELS + cid, sizeof(*shared));
	if (!shared) {
		cprintf("smd_alloc_channel() cid %d does not exist\n", cid);
		return;
	}

	ch = calloc(1, sizeof(struct smd_channel));
	if (ch == 0) {
		cprintf("smd_alloc_channel() out of memory\n");
		return;
	}

	ch->send = &shared->ch0;
	ch->recv = &shared->ch1;
	ch->n = cid;

	if (smd_is_packet(cid)) {
		ch->read = smd_packet_read;
		ch->write = smd_packet_write;
		ch->read_avail = smd_packet_read_avail;
		ch->write_avail = smd_packet_write_avail;
		ch->update_state = update_packet_state;
	} else {
		ch->read = smd_stream_read;
		ch->write = smd_stream_write;
		ch->read_avail = smd_stream_read_avail;
		ch->write_avail = smd_stream_write_avail;
		ch->update_state = update_stream_state;
	}

	memcpy(ch->name, "SMD_", 4);
	memcpy(ch->name + 4, name, 20);
	ch->name[23] = 0;

	cprintf("smd_alloc_channel() '%s' cid=%d, shared=%p\n",
		ch->name, ch->n, shared);

	pthread_mutex_lock(&smd_creation_mutex);
	LIST_INSERT_HEAD(&smd_ch_closed_list, ch, ch_list);
	pthread_mutex_unlock(&smd_creation_mutex);
}

static void do_nothing_notify(void *priv, unsigned flags)
{
}

static struct smd_channel *smd_get_channel(const char *name)
{
	struct smd_channel *ch, *next;

	pthread_mutex_lock(&smd_creation_mutex);
	for (ch = LIST_FIRST(&smd_ch_closed_list); ch != NULL; ch = next) {
                next = LIST_NEXT(ch, ch_list);

		if (!strcmp(name, ch->name)) {
			LIST_REMOVE(ch, ch_list);
			pthread_mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	pthread_mutex_unlock(&smd_creation_mutex);

	return NULL;
}

int smd_open(const char *name, smd_channel_t **_ch,
	     void *priv, void (*notify)(void *, unsigned))
{
	struct smd_channel *ch;

	if (smd_initialized == 0) {
		cprintf("smd_open() before smd_init()\n");
		return -E_NOT_FOUND;
	}

	D("smd_open('%s', %p, %p)\n", name, priv, notify);

	ch = smd_get_channel(name);
	if (!ch)
		return -E_NOT_FOUND;

	if (notify == 0)
		notify = do_nothing_notify;

	ch->notify = notify;
	ch->current_packet = 0;
	ch->last_state = SMD_SS_CLOSED;
	ch->priv = priv;

	*_ch = ch;

	D("smd_open: opening '%s'\n", ch->name);

	pthread_mutex_lock(&smd_mutex);
	LIST_INSERT_HEAD(&smd_ch_list, ch, ch_list);

	/* If the remote side is CLOSING, we need to get it to
	 * move to OPENING (which we'll do by moving from CLOSED to
	 * OPENING) and then get it to move from OPENING to
	 * OPENED (by doing the same state change ourselves).
	 *
	 * Otherwise, it should be OPENING and we can move directly
	 * to OPENED so that it will follow.
	 */
	if (ch->recv->state == SMD_SS_CLOSING) {
		ch->send->head = 0;
		hc_set_state(ch->send, SMD_SS_OPENING);
	} else {
		hc_set_state(ch->send, SMD_SS_OPENED);
	}

	pthread_mutex_unlock(&smd_mutex);
	smd_kick(ch);

	D("smd_open('%s', %p, %p) ch=%p\n", ch->name, priv, notify, ch);

	return 0;
}

int smd_close(smd_channel_t *ch)
{
	cprintf("smd_close(%p)\n", ch);

	if (ch == 0)
		return -1;

	pthread_mutex_lock(&smd_mutex);
	ch->notify = do_nothing_notify;
	LIST_REMOVE(ch, ch_list);
	hc_set_state(ch->send, SMD_SS_CLOSED);
	pthread_mutex_unlock(&smd_mutex);

	pthread_mutex_lock(&smd_creation_mutex);
	LIST_INSERT_HEAD(&smd_ch_closed_list, ch, ch_list);
	pthread_mutex_unlock(&smd_creation_mutex);

	return 0;
}

int smd_read(smd_channel_t *ch, void *data, int len)
{
	return ch->read(ch, data, len);
}

int smd_write(smd_channel_t *ch, const void *data, int len)
{
	return ch->write(ch, data, len);
}

int smd_read_avail(smd_channel_t *ch)
{
	return ch->read_avail(ch);
}

int smd_write_avail(smd_channel_t *ch)
{
	return ch->write_avail(ch);
}

#if 0
int smd_wait_until_readable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_wait_until_writable(smd_channel_t *ch, int bytes)
{
	return -1;
}
#endif

int smd_cur_packet_size(smd_channel_t *ch)
{
	return ch->current_packet;
}


/* --------------------------------------------------------------------------------------- */

volatile void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}

static volatile void *_smem_find(unsigned id, unsigned *size)
{
	volatile struct smem_shared *shared = (volatile void *) MSM_SHARED_RAM_BASE;
	volatile struct smem_heap_entry *toc = shared->heap_toc;

	if (id >= SMEM_NUM_ITEMS)
		return 0;

	if (toc[id].allocated) {
		*size = toc[id].size;
		return (volatile void *) (MSM_SHARED_RAM_BASE + toc[id].offset);
	}

	return 0;
}

volatile void *smem_find(unsigned id, unsigned size_in)
{
	unsigned size;
	volatile void *ptr;

	ptr = _smem_find(id, &size);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		cprintf("smem_find(%d, %d): wrong size %d\n",
		       id, size_in, size);
		return 0;
	}

	return ptr;
}

static irqreturn_t smsm_irq_handler(int irq, void *data)
{
	volatile struct smsm_shared *smsm;

	pthread_mutex_lock(&smem_mutex);
	smsm = smem_alloc(ID_SHARED_STATE,
			  2 * sizeof(struct smsm_shared));

	if (smsm == 0) {
		cprintf("<SM NO STATE>\n");
	} else {
		unsigned apps = smsm[0].state;
		unsigned modm = smsm[1].state;

		if (msm_smd_debug_mask & MSM_SMSM_DEBUG)
			cprintf("<SM %08x %08x>\n", apps, modm);
		if (modm & SMSM_RESET) {
			handle_modem_crash();
		} else {
			apps |= SMSM_INIT;
			if (modm & SMSM_SMDINIT)
				apps |= SMSM_SMDINIT;
			if (modm & SMSM_RPCINIT)
				apps |= SMSM_RPCINIT;
		}

		if (smsm[0].state != apps) {
			if (msm_smd_debug_mask & MSM_SMSM_DEBUG)
				cprintf("<SM %08x NOTIFY>\n", apps);
			smsm[0].state = apps;
			do_smd_probe();
			notify_other_smsm();
		}
	}
	pthread_mutex_unlock(&smem_mutex);
	return IRQ_HANDLED;
}

int smsm_change_state(uint32_t clear_mask, uint32_t set_mask)
{
	volatile struct smsm_shared *smsm;

	pthread_mutex_lock(&smem_mutex);

	smsm = smem_alloc(ID_SHARED_STATE,
			  2 * sizeof(struct smsm_shared));

	if (smsm) {
		if (smsm[1].state & SMSM_RESET)
			handle_modem_crash();
		smsm[0].state = (smsm[0].state & ~clear_mask) | set_mask;
		if (msm_smd_debug_mask & MSM_SMSM_DEBUG)
			cprintf("smsm_change_state %x\n",
			       smsm[0].state);
		notify_other_smsm();
	}

	pthread_mutex_unlock(&smem_mutex);

	if (smsm == NULL) {
		cprintf("smsm_change_state <SM NO STATE>\n");
		return -E_IO;
	}
	return 0;
}

uint32_t smsm_get_state(void)
{
	volatile struct smsm_shared *smsm;
	uint32_t rv;

	pthread_mutex_lock(&smem_mutex);

	smsm = smem_alloc(ID_SHARED_STATE,
			  2 * sizeof(struct smsm_shared));

	if (smsm)
		rv = smsm[1].state;
	else
		rv = 0;

	if (rv & SMSM_RESET)
		handle_modem_crash();

	pthread_mutex_unlock(&smem_mutex);

	if (smsm == NULL)
		cprintf("smsm_get_state <SM NO STATE>\n");
	return rv;
}

int smsm_set_sleep_duration(uint32_t delay)
{
	volatile uint32_t *ptr;

	ptr = smem_alloc(SMEM_SMSM_SLEEP_DELAY, sizeof(*ptr));
	if (ptr == NULL) {
		cprintf("smsm_set_sleep_duration <SM NO SLEEP_DELAY>\n");
		return -E_IO;
	}
	if (msm_smd_debug_mask & MSM_SMSM_DEBUG)
		cprintf("smsm_set_sleep_duration %d -> %d\n",
		       *ptr, delay);
	*ptr = delay;
	return 0;
}

int smsm_set_interrupt_info(struct smsm_interrupt_info *info)
{
	volatile struct smsm_interrupt_info *ptr;

	ptr = smem_alloc(SMEM_SMSM_INT_INFO, sizeof(*ptr));
	if (ptr == NULL) {
		cprintf("smsm_set_sleep_duration <SM NO INT_INFO>\n");
		return -E_IO;
	}
	if (msm_smd_debug_mask & MSM_SMSM_DEBUG)
		cprintf("smsm_set_interrupt_info %x %x -> %x %x\n",
		       ptr->aArm_en_mask, ptr->aArm_interrupts_pending,
		       info->aArm_en_mask, info->aArm_interrupts_pending);
	*ptr = *info;
	return 0;
}

#define MAX_NUM_SLEEP_CLIENTS       64
#define MAX_SLEEP_NAME_LEN          8

#define NUM_GPIO_INT_REGISTERS 6
#define GPIO_SMEM_NUM_GROUPS 2
#define GPIO_SMEM_MAX_PC_INTERRUPTS 8
struct tramp_gpio_save
{
  unsigned int enable;
  unsigned int detect;
  unsigned int polarity;
};
struct tramp_gpio_smem
{
	uint16_t num_fired[GPIO_SMEM_NUM_GROUPS];
	uint16_t fired[GPIO_SMEM_NUM_GROUPS][GPIO_SMEM_MAX_PC_INTERRUPTS];
	uint32_t enabled[NUM_GPIO_INT_REGISTERS];
	uint32_t detection[NUM_GPIO_INT_REGISTERS];
	uint32_t polarity[NUM_GPIO_INT_REGISTERS];
};


void smsm_print_sleep_info(void)
{
	volatile uint32_t *ptr;
	volatile struct tramp_gpio_smem *gpio;
	volatile struct smsm_interrupt_info *int_info;


	pthread_mutex_lock(&smem_mutex);

	ptr = smem_alloc(SMEM_SMSM_SLEEP_DELAY, sizeof(*ptr));
	if (ptr)
		cprintf("SMEM_SMSM_SLEEP_DELAY: %x\n", *ptr);
	else
		cprintf("SMEM_SMSM_SLEEP_DELAY: missing\n");

	ptr = smem_alloc(SMEM_SMSM_LIMIT_SLEEP, sizeof(*ptr));
	if (ptr)
		cprintf("SMEM_SMSM_LIMIT_SLEEP: %x\n", *ptr);
	else
		cprintf("SMEM_SMSM_LIMIT_SLEEP: missing\n");

	ptr = smem_alloc(SMEM_SLEEP_POWER_COLLAPSE_DISABLED, sizeof(*ptr));
	if (ptr)
		cprintf("SMEM_SLEEP_POWER_COLLAPSE_DISABLED: %x\n", *ptr);
	else
		cprintf("SMEM_SLEEP_POWER_COLLAPSE_DISABLED: missing\n");

	int_info = smem_alloc(SMEM_SMSM_INT_INFO, sizeof(*int_info));
	if (int_info)
		cprintf("SMEM_SMSM_INT_INFO %x %x %x\n",
		       int_info->aArm_en_mask, int_info->aArm_interrupts_pending, int_info->aArm_wakeup_reason);
	else
		cprintf("SMEM_SMSM_INT_INFO: missing\n");

	gpio = smem_alloc( SMEM_GPIO_INT, sizeof(*gpio)); 
	if (gpio) {
		int i;
		for(i = 0; i < NUM_GPIO_INT_REGISTERS; i++) {
			cprintf("SMEM_GPIO_INT: %d: e %x d %x p %x\n",
			       i, gpio->enabled[i], gpio->detection[i],
			       gpio->polarity[i]);
		}
		for(i = 0; i < GPIO_SMEM_NUM_GROUPS; i++) {
			cprintf("SMEM_GPIO_INT: %d: f %d: %d %d...\n",
			       i, gpio->num_fired[i], gpio->fired[i][0], gpio->fired[i][1]);
		}
	}
	else
		cprintf("SMEM_GPIO_INT: missing\n");
	
#if 0
	ptr = smem_alloc( SMEM_SLEEP_STATIC, 
                                           2 * MAX_NUM_SLEEP_CLIENTS * 
                                           ( MAX_SLEEP_NAME_LEN + 1 ) ); 
	if (ptr)
		cprintf("SMEM_SLEEP_STATIC: %x %x %x %x\n", ptr[0], ptr[1], ptr[2], ptr[3]);
	else
		cprintf("SMEM_SLEEP_STATIC: missing\n");
#endif

	pthread_mutex_unlock(&smem_mutex);
}

static void irq_wait(uint32_t irq, irqreturn_t (*handler)(int, void *))
{
	int64_t last = -1;

	cprintf("smd irq thread started (irq %u)\n", irq);
	jos_atomic_dec(&irq_thread_semaphore);

	while (1) {
		last = sys_irq_wait(irq, last);
cprintf("\n\n\n----\nIRQ %d\n\n\n----\n", irq);
		pthread_mutex_lock(&irq_mutex);
		handler(irq, NULL);
		pthread_mutex_unlock(&irq_mutex);
	}
}

static void *smd_irq_thread(void *arg)
{
	irq_wait(INT_A9_M2A_0, smd_irq_handler);
	return NULL;
}

static void *smsm_irq_thread(void *arg)
{
	irq_wait(INT_A9_M2A_5, smsm_irq_handler);
	return NULL;
}

static int smd_core_init(void)
{
	int fd;
	cprintf("smd_core_init()\n");

	fd = open(smd_segment_device, O_RDWR);
	if (fd == -1) {
		cprintf("%s: failed to open smd segment at %s\n",
		    __func__, smd_segment_device);
		exit(1);
	}
	smd_segment = mmap(NULL, smd_segment_len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (smd_segment == NULL) {
		cprintf("%s: failed to mmap smd segment\n", __func__);
		exit(1);
	}

	fd = open(notify_segment_device, O_RDWR);
	if (fd == -1) {
		cprintf("%s: failed to open notify segment at %s\n",
		    __func__, notify_segment_device);
		exit(1);
	}
	notify_segment = mmap(NULL, notify_segment_len, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, 0);
	if (notify_segment == NULL) {
		cprintf("%s: failed to mmap notify segment\n", __func__);
		exit(1);
	}

	D("%s(): mapped smd @ %p, notify @ %p\n", __func__, smd_segment,
	    notify_segment);

	/* we may have missed a signal while booting -- fake
	 * an interrupt to make sure we process any existing
	 * state
	 */
	smsm_irq_handler(0, 0);

	cprintf("smd_core_init() done\n");

	return 0;
}

int msm_smd_init()
{
	pthread_t tid;

	cprintf("smd_init()\n");

	pthread_mutex_init(&smd_mutex, NULL);
	pthread_mutex_init(&smem_mutex, NULL);
	pthread_mutex_init(&smd_creation_mutex, NULL);

	if (smd_core_init()) {
		cprintf("smd_core_init() failed\n");
		return -1;
	}

	do_smd_probe();

	//msm_check_for_modem_crash = check_for_modem_crash;

	smd_initialized = 1;

	pthread_mutex_init(&irq_mutex, NULL);
	pthread_create(&tid, NULL, smd_irq_thread, NULL);
	pthread_create(&tid, NULL, smsm_irq_thread, NULL);

	// wait for irq threads to have started
	do {
		usleep(100000);
	} while (irq_thread_semaphore.counter > 0);

	return 0;
}
