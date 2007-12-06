#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/jthread.h>
#include <inc/intmacro.h>
#include <machine/memlayout.h>

#include <string.h>
#include <stdio.h>

#include <archcall.h>
#include <os-jos64/lutrap.h>
#include <inc/string.h>

#include "jif.h"

#define JIF_BUFS	64

static struct cobj_ref the_netdev;

struct jif_data {
    struct cobj_ref ndev;

    int64_t waiter_id;
    int64_t waitgen;
    
    struct netbuf_hdr *rx[JIF_BUFS];
    struct netbuf_hdr *tx[JIF_BUFS];
    
    int rx_head;	/* kernel will place next packet here */
    int rx_tail;	/* last buffer we gave to the kernel */
    int tx_next;	/* next slot to use for TX */

    uint64_t shutdown;

    struct cobj_ref buf_seg;
    void *buf_base;

    volatile uint64_t irq_flag;
    jthread_mutex_t rx_mu;
};

static void
jif_lock(struct jif_data *jif)
{
    jthread_mutex_lock(&jif->rx_mu);
}

static void
jif_unlock(struct jif_data *jif)
{
    jthread_mutex_unlock(&jif->rx_mu);
}

static int
get_netdev(const char *name, struct cobj_ref *co)
{
    *co = the_netdev;
    return 0;
}

static void
jif_rxbuf_feed(struct jif_data *jif)
{
    int r, i;
    int ss = (jif->rx_tail >= 0 ? (jif->rx_tail + 1) % JIF_BUFS : 0);
    for (i = ss; i != jif->rx_head; i = (i + 1) % JIF_BUFS) {
	void *bufaddr = jif->rx[i];
	jif->rx[i]->actual_count = 0;
	r = sys_net_buf(jif->ndev, jif->buf_seg,
			(uint64_t) (bufaddr - jif->buf_base), netbuf_rx);
	if (r < 0) {
	    cprintf("jif: cannot feed rx packet: %s\n", e2s(r));
	    break;
	}
	
	jif->rx_tail = i;
	if (jif->rx_head == -1)
	    jif->rx_head = i;
    }
}    

static void
jif_rx_thread(void *a)
{
    struct jif_data *jif = a;

    struct cobj_ref base_as;
    int r = sys_self_get_as(&base_as);
    if (r < 0) {
	cprintf("jif_rx_thread: cannot get AS\n");
	sys_self_halt();
    }

    int64_t rx_asid = sys_as_copy(base_as, start_env->proc_container, 0, "jif_rx_thread as");
    if (rx_asid < 0) {
	cprintf("jif_rx_thread: cannot copy AS\n");
	sys_self_halt();
    }

    sys_self_set_as(COBJ(start_env->proc_container, rx_asid));
    segment_as_switched();

    // set waiter_id, reset any previous buffers
    jif->waitgen = sys_net_wait(jif->ndev, jif->waiter_id, 0);

    while (!jif->shutdown) {
	jif_lock(jif);
	while (!jif->shutdown && (jif->rx_head < 0 || !(jif->rx[jif->rx_head]->actual_count & NETHDR_COUNT_DONE))) {
	    jif_rxbuf_feed(jif);

	    jif_unlock(jif);
	    jif->waitgen = sys_net_wait(jif->ndev, jif->waiter_id, jif->waitgen);
	    jif_lock(jif);

	    if (jif->waitgen == -E_AGAIN) {
		int i;
		// All buffers have been cleared
		for (i = 0; i < JIF_BUFS; i++)
		    jif->tx[i]->actual_count = NETHDR_COUNT_DONE;
		jif->rx_head = -1;
		jif->rx_tail = -1;
	    }
	} 
	jif_unlock(jif);
	jif->irq_flag = 1;
	lutrap_kill(SIGNAL_ETH);
	while (!jif->shutdown && jif->irq_flag)
	    sys_sync_wait(&jif->irq_flag, 1, UINT64(~0));
    }

    jif->shutdown = 2;
    sys_sync_wakeup(&jif->shutdown);
}

int
jif_rx(void *data, void *buf, unsigned int buf_len)
{
    int slot;
    uint16_t count, len;
    void *rxbuf;
    struct jif_data *jif = data;

    jif_lock(jif);
    if (jif->rx_head < 0 || 
	!(jif->rx[jif->rx_head]->actual_count & NETHDR_COUNT_DONE)) {
	jif_unlock(jif);
	return 0;
    }

    slot = jif->rx_head;
    if (jif->rx_head == jif->rx_tail)
	jif->rx_head = -1;
    else
	jif->rx_head = (jif->rx_head + 1) % JIF_BUFS;

    count = jif->rx[slot]->actual_count;
    if ((count & NETHDR_COUNT_ERR)) {
	cprintf("jif: rx packet error\n");
	jif_rxbuf_feed(jif);
	jif_unlock(jif);
	return 0;
    }

    rxbuf = (void *) (jif->rx[slot] + 1);
    len = count & NETHDR_COUNT_MASK;
    if (len > buf_len) {
	cprintf("jif: not enough space (%d > %d)\n", len, buf_len);
	jif_rxbuf_feed(jif);
	jif_unlock(jif);
	return 0;
    }
    memcpy(buf, rxbuf, len);
    
    jif_rxbuf_feed(jif);
    
    jif_unlock(jif);
    return len;
}

int
jif_tx(void *data, void *buf, unsigned int buf_len)
{
    int r, txsize, txslot;
    struct jif_data *jif = data;
    void *txbase;
    char *txbuf;

    txslot = jif->tx_next;
    if (!(jif->tx[txslot]->actual_count & NETHDR_COUNT_DONE)) {
	cprintf("jif: out of tx bufs\n");
	return -1;
    }

    jif->tx_next = (jif->tx_next + 1) % JIF_BUFS;
    txbuf = (char *) (jif->tx[txslot] + 1);
    txsize = buf_len;

    if (buf_len > 2000)
	panic("oversized packet, txsize %d\n", txsize);
    memcpy(txbuf, buf, buf_len);
    
    txbase = jif->tx[txslot];
    jif->tx[txslot]->size = txsize;
    jif->tx[txslot]->actual_count = 0;

    r = sys_net_buf(jif->ndev, jif->buf_seg,
		    (uint64_t) (txbase - jif->buf_base),
		    netbuf_tx);
    if (r < 0) {
	cprintf("jif: can't setup tx slot: %s\n", e2s(r));
	return -1;
    }

    return buf_len;
}

int
jif_open(const char *name, void *data)
{
    int r, i;
    struct cobj_ref to;
    struct jif_data *jif = data;

    jif->shutdown = 0;

    r = get_netdev(name, &jif->ndev);
    if (r < 0) {
	arch_printf("jif_open: get_netdev failed: %s\n", e2s(r));
	return -1;
    }
    
    // Allocate transmit/receive pages
    jif->waitgen = -1;
    jif->waiter_id = sys_pstate_timestamp();
    if (jif->waiter_id < 0)
	panic("jif: cannot get thread id: %s", e2s(jif->waiter_id));

    // container gets passed as the argument to _start()
    uint64_t container = start_env->proc_container;

    jif->buf_base = 0;
    r = segment_alloc(container, JIF_BUFS * PGSIZE,
		      &jif->buf_seg, &jif->buf_base,
		      0, "jif rx/tx buffers");
    if (r < 0)
	panic("jif: cannot allocate %d buffer pages: %s\n",
	      JIF_BUFS, e2s(r));

    for (i = 0; i < JIF_BUFS; i++) {
	jif->rx[i] = jif->buf_base + i * PGSIZE;
	jif->rx[i]->size = 2000;
	jif->rx[i]->actual_count = -1;

	jif->tx[i] = jif->buf_base + i * PGSIZE + 2048;
	jif->tx[i]->actual_count = NETHDR_COUNT_DONE;
    }

    jif->tx_next = 0;
    jif->rx_head = -1;
    jif->rx_tail = -1;

    jif->irq_flag = 0;
    
    jthread_mutex_init(&jif->rx_mu);

    r = thread_create(start_env->proc_container, &jif_rx_thread, jif, &to, "rx thread");
    if (r < 0) {
	arch_printf("jif_open: thread_create failed: %s\n", e2s(r));
	return -1;
    }
    
    return 0;
}

int
jif_close(void *data)
{
    struct jif_data *jif = data;
    jif->shutdown = 1;

    while (jif->shutdown == 1) {
	sys_net_wait(jif->ndev, sys_pstate_timestamp(), 0);
	sys_sync_wakeup(&jif->irq_flag);
	sys_sync_wait(&jif->shutdown, 1, sys_clock_nsec() + 100000000);
    }

    return 0;
}

int
jif_list(struct jif_list *list, unsigned int cnt)
{
    char name[KOBJ_NAME_LEN];
    int r = sys_obj_get_name(the_netdev, &name[0]);
    if (r < 0)
	return r;

    strncpy(list[0].name, name, sizeof(list[0].name));
    r = sys_net_macaddr(the_netdev, list[0].mac);
    if (r < 0)
	return r;

    list[0].data_len = sizeof(struct jif_data);
    return 1;
}

void
jif_irq_reset(void *data)
{
    struct jif_data *jif = data;
    jif->irq_flag = 0;
    sys_sync_wakeup(&jif->irq_flag);
}

int
jif_set_netdev(char *str)
{
    char buf[64];
    char *dot;

    snprintf(&buf[0], sizeof(buf), "%s", str);

    dot = strchr(buf, '.');
    if (!dot)
	panic("set_netdev: bad cobj_ref string %s\n", buf);

    *dot = '\0';
    int r1 = strtou64(buf, 0, 10, &the_netdev.container);
    int r2 = strtou64(dot + 1, 0, 10, &the_netdev.object);
    if (r1 < 0 || r2 < 0)
	panic("unable to parse netdev %s: %d %d\n", str, r1, r2);

    return 1;
}
