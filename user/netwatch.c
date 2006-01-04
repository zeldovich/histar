#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/error.h>

static void
register_rxbufs(struct cobj_ref seg, struct netbuf_hdr **rx)
{
    for (int i = 0; i < 8; i++) {
	rx[i]->actual_count = 0;
	int r = sys_net_buf(seg, i * PGSIZE, netbuf_rx);
	if (r < 0)
	    cprintf("cannot register rx buffer #%d: %s\n", i, e2s(r));
    }
}

int
main(int ac, char **av)
{
    uint64_t ctemp = start_env->container;

    struct cobj_ref seg;
    void *va = 0;
    int r = segment_alloc(ctemp, 8 * PGSIZE, &seg, &va);
    if (r < 0)
	panic("cannot allocate buffer segment: %s", e2s(r));

    struct netbuf_hdr *rx[8];
    for (int i = 0; i < 8; i++) {
	rx[i] = va + i * PGSIZE;
	rx[i]->size = 2000;
    }

    register_rxbufs(seg, rx);

    uint8_t mac[6];
    r = sys_net_macaddr(&mac[0]);
    if (r < 0)
	panic("cannot get MAC address: %s", e2s(r));

    cprintf("net: card address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    int64_t waitgen = 0;
    int64_t waiter_id = thread_id();
    if (waiter_id < 0)
	panic("cannot get thread id: %s", e2s(waiter_id));

    for (;;) {
	waitgen = sys_net_wait(waiter_id, waitgen);
	if (waitgen == -E_AGAIN)
	    register_rxbufs(seg, rx);

	for (int i = 0; i < 8; i++) {
	    if ((rx[i]->actual_count & NETHDR_COUNT_DONE)) {
		unsigned char *buf = (unsigned char *) (rx[i] + 1);
		cprintf("[%d bytes] %02x:%02x:%02x:%02x:%02x:%02x > "
		        "%02x:%02x:%02x:%02x:%02x:%02x type %02x%02x\n",
			rx[i]->actual_count & NETHDR_COUNT_MASK,
			buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[12], buf[13]);

		rx[i]->actual_count = 0;
		r = sys_net_buf(seg, i * PGSIZE, netbuf_rx);
		if (r < 0)
		    cprintf("cannot re-register rx buffer: %s\n", e2s(r));
	    }
	}
    }
}
