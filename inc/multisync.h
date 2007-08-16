#ifndef JOS_INC_MULTISYNC_H
#define JOS_INC_MULTISYNC_H

#include <inc/container.h>

typedef enum
{
    dev_probe_read = 0,
    dev_probe_write,    
} dev_probe_t;

struct wait_stat;
typedef int (*multisync_pre)(struct wait_stat *, dev_probe_t,
			     volatile uint64_t **addrp, void **arg1);
typedef int (*multisync_post)(struct wait_stat *, void *arg1, dev_probe_t);

struct wait_stat
{
    char ws_type;

    union {
	char ws_cbbuf[24];
	void *ws_cbarg;
    };
    dev_probe_t ws_probe;
    multisync_pre ws_cb0;
    multisync_post ws_cb1;

    union {
	struct {
	    struct cobj_ref ws_seg;
	    uint64_t ws_off;
	};
	struct {
	    uint64_t ws_refct;
	    volatile uint64_t *ws_addr;
	};
    };
    uint64_t ws_val;
};

#define WS_ISOBJ(__ws) ((__ws)->ws_type == 0)
#define WS_ISADDR(__ws) ((__ws)->ws_type == 1)
#define WS_ISASS(__ws) ((__ws)->ws_type == 2)

#define WS_SETOBJ(__ws, __seg, __off) \
    do {			      \
	(__ws)->ws_type = 0;	      \
	(__ws)->ws_seg = __seg;	      \
	(__ws)->ws_off = __off;	      \
    } while (0)
#define WS_SETADDR(__ws, __addr)      \
    do {			      \
	(__ws)->ws_type = 1;	      \
	(__ws)->ws_refct = 0;	      \
	(__ws)->ws_addr = __addr;     \
    } while (0)
#define WS_SETASS(__ws) (__ws)->ws_type = 2

#define WS_SETVAL(__ws, __val) (__ws)->ws_val = __val

#define WS_SETCB0(__ws, __cb) (__ws)->ws_cb0 = __cb
#define WS_SETCB1(__ws, __cb) (__ws)->ws_cb1 = __cb

int multisync_wait(struct wait_stat *wstat, uint64_t n, uint64_t nsec);

#endif
