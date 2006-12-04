extern "C" {
#include <inc/lib.h>
#include <inc/segment.h>
#include <inc/multisync.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <errno.h>
#include <string.h>
}

#include <inc/error.hh>

extern "C" int
multisync_wait(struct wait_stat *wstat, uint64_t n, uint64_t msec)
{
    int r = 0;
    volatile uint64_t *addrs[n];
    uint64_t vals[n];
    // for assigning wait addresses
    uint64_t spares[n];
    uint64_t spares_used = 0;
    memset(spares, 0, sizeof(spares));
    // store cb1 args
    void *args[n];
    // store mapped segments
    uint64_t *mapped[n];
    uint64_t mapped_count = 0;
    try {
	for (uint64_t i = 0; i < n; i++) {
	    if (WS_ISOBJ(&wstat[i])) {
		// map the segment to pass an address to the kernel, unmap
		// segments when wait returns.
		uint8_t *addr = 0;		
		error_check(segment_map(wstat[i].ws_seg, 0,
					SEGMAP_READ,
					(void **) &addr, 0, 0));
		mapped[mapped_count] = (uint64_t *)addr;
		mapped_count++;
		addr += wstat[i].ws_off;
		addrs[i] = (uint64_t *)addr;
		vals[i] = wstat[i].ws_val;
	    } else if (WS_ISASS(&wstat[i])) {
		// assign address
		addrs[i] = &spares[spares_used++];
		vals[i] = 0;
	    } else {
		// given address
		addrs[i] = wstat[i].ws_addr;
		vals[i] = wstat[i].ws_val;
	    }

	}
	
	for (uint64_t i = 0; i < n; i++) {
	    if (wstat[i].ws_cb0) {
		r = (*wstat[i].ws_cb0)(wstat[i].ws_cbarg, wstat[i].ws_probe, 
				       addrs[i], &args[i]);
		if (r < 0)
		    cprintf("multisync_wait: cb0 error: %s\n", e2s(r));
	    }
	}
	
	r = sys_sync_wait_multi(addrs, vals, n, msec);
	if (r == -E_NO_SPACE) {
	    error_check(sys_self_set_waitslots(n));
	    error_check(sys_sync_wait_multi(addrs, vals, n, msec));
	}
	else if (r < 0)
	    throw error(r, "sys_sync_wait_multi error");
    } catch (basic_exception &e) {
	cprintf("multisync_wait: error: %s\n", e.what());
	errno = EIO;
	r = -1;
    }
    
    for (uint64_t i = 0; i < n; i++) {
	if (wstat[i].ws_cb1) {
	    r = (*wstat[i].ws_cb1)(wstat[i].ws_cbarg, args[i], wstat[i].ws_probe);
	    if (r < 0)
		cprintf("multisync_wait: cb1 error: %s\n", e2s(r));
	}
    }
    
    for (uint64_t i = 0; i < mapped_count; i++)
	segment_unmap_delayed(mapped[i], 1);
    
    return r;
}
