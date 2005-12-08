#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>

int
main(int ac, char **av)
{
    // hard-coded root container
    uint64_t ctemp = 1;

    struct cobj_ref seg;
    int r = segment_alloc(ctemp, 8 * PGSIZE, &seg);
    if (r < 0)
	panic("cannot allocate buffer segment: %s", e2s(r));

    cprintf("allocated segment %ld:%ld\n", seg.container, seg.slot);

    void *va;
    r = segment_map(ctemp, seg, 1, &va, 0);
    if (r < 0)
	panic("cannot map buffer segment: %s", e2s(r));

    struct netbuf_hdr *rx[8];
    struct netbuf_hdr *tx[8];
    for (int i = 0; i < 8; i++) {
	rx[i] = va + i * PGSIZE;
	rx[i]->size = 2000;
	rx[i]->actual_count = 0;
	r = sys_net_buf(seg, i, 0, netbuf_rx);
	if (r < 0)
	    panic("cannot register rx buffer: %s", e2s(r));

	tx[i] = va + i * PGSIZE + 2048;
	tx[i]->size = 1000;
	tx[i]->actual_count = 0;
	//r = sys_net_buf(seg, i, 2048, netbuf_tx);
	r = 0;
	if (r < 0)
	    panic("cannot register tx buffer: %s", e2s(r));
    }

    int64_t waitgen = 0;
    for (;;) {
	waitgen = sys_net_wait(waitgen);
	//cprintf("net: woken up, waitgen %ld\n", waitgen);

	for (int i = 0; i < 8; i++) {
	    if ((rx[i]->actual_count & NETHDR_COUNT_DONE)) {
		unsigned char *buf = (unsigned char *) (rx[i] + 1);
		cprintf("[%d bytes] %02x:%02x:%02x:%02x:%02x:%02x > %02x:%02x:%02x:%02x:%02x:%02x type %02x%02x\n",
			rx[i]->actual_count & NETHDR_COUNT_MASK,
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
			buf[6], buf[7], buf[8], buf[9], buf[10], buf[11],
			buf[12], buf[13]);

		rx[i]->actual_count = 0;
		r = sys_net_buf(seg, i, 0, netbuf_rx);
		if (r < 0)
		    cprintf("cannot re-register rx buffer: %s", e2s(r));
	    }
	}
    }
}
