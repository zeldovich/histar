#include <inc/memlayout.h>
#include <inc/syscall.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>

int
main(int ac, char **av)
{
    // hard-coded root container
    uint64_t ctemp = 1;

    struct cobj_ref seg;
    int r = segment_alloc(ctemp, 9 * PGSIZE, &seg);
    if (r < 0)
	panic("cannot allocate buffer segment: %s", e2s(r));

    cprintf("allocated segment %ld:%ld\n", seg.container, seg.slot);

    void *va;
    r = segment_map(ctemp, seg, 1, &va, 0);
    if (r < 0)
	panic("cannot map buffer segment: %s", e2s(r));

    struct netbuf_hdr *rx[8];
    struct netbuf_hdr *tx;
    for (int i = 0; i < 8; i++) {
	rx[i] = va + i * PGSIZE;
	rx[i]->size = 2000;
	rx[i]->actual_count = 0;
	r = sys_net_buf(seg, i * PGSIZE, netbuf_rx);
	if (r < 0)
	    panic("cannot register rx buffer: %s", e2s(r));
    }

    uint8_t mac[6];
    r = sys_net_macaddr(&mac[0]);
    if (r < 0)
	panic("cannot get MAC address: %s", e2s(r));

    cprintf("net: card address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    tx = va + 8 * PGSIZE;
    tx->size = 256;
    tx->actual_count = 0;
    unsigned char *txbuf = (unsigned char *) (tx + 1);

    int64_t waitgen = 0;
    for (;;) {
	waitgen = sys_net_wait(waitgen);
	//cprintf("net: woken up, waitgen %ld\n", waitgen);

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

		if (tx->actual_count == 0) {
		    txbuf[0] = 0; txbuf[1] = 0x7; txbuf[2] = 0xe9;
		    txbuf[3] = 0xf; txbuf[4] = 0x1f; txbuf[5] = 0x3e;

		    memcpy(&txbuf[6], &mac[0], 6);
		    txbuf[12] = 0x8; txbuf[13] = 0x6;

		    tx->actual_count = 1;	// just to flag it as busy
		    int r = sys_net_buf(seg, 8 * PGSIZE, netbuf_tx);
		    if (r < 0)
			cprintf("cannot transmit packet: %s\n", e2s(r));
		    else
			cprintf("Transmitting packet\n");
		}
	    }
	}

	if ((tx->actual_count & NETHDR_COUNT_DONE)) {
	    cprintf("tx complete\n");
	    tx->actual_count = 0;
	}
    }
}
