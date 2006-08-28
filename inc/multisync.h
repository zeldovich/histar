#ifndef JOS_INC_MULTISYNC_H
#define JOS_INC_MULTISYNC_H

#include <inc/container.h>

typedef enum
{
    dev_probe_read,
    dev_probe_write,    
} dev_probe_t;

typedef int (*multisync_pre)(void *arg0, dev_probe_t probe, void **arg1);
typedef int (*multisync_post)(void *arg0, void *arg1, dev_probe_t probe);

struct wait_stat
{
    char ws_isobj;

    void *ws_cbarg;
    multisync_pre ws_cb0;
    multisync_post ws_cb1;
    
    dev_probe_t ws_probe;
    union {
	struct {
	    struct cobj_ref ws_seg;
	    uint64_t ws_off;
	};
	volatile uint64_t *ws_addr;
    };
    uint64_t ws_val;


};

int multisync_wait(struct wait_stat *wstat, uint64_t n, uint64_t msec);

#endif
