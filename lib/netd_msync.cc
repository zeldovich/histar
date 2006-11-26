extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/netdmsync.h>
#include <inc/stdio.h>
#include <inc/atomic.h>
#include <inc/error.h>
#include <inc/syscall.h>
#include <inc/debug.h>

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
}

#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static const char dbg = 0;

#define SOCK_SEL_MAP(__seg, __va)				\
    do {							\
	int __r;						\
	__r = segment_map(__seg, 0,				\
			  SEGMAP_READ | SEGMAP_WRITE,		\
			  (void **)(__va), 0, 0);		\
	if (__r < 0) {						\
	    cprintf("%s: cannot segment_map: %s\n",		\
		    __FUNCTION__, e2s(__r));			\
	    return __r;						\
	}							\
    } while(0)

struct select_worker_arg {
    struct cobj_ref seg;
    char op;
};

static void
sock_statsync_worker(void *arg)
{
    struct select_worker_arg *args = (struct select_worker_arg*) arg;

    struct cobj_ref s = args->seg;
    char op = args->op;
    free(args);

    netd_select_init(s, op);
}

static int
sock_statsync_cb0(void *arg0, dev_probe_t probe, volatile uint64_t *addr, 
		  void **arg1)
{
    struct cobj_ref seg = *((struct cobj_ref *)arg0);

    struct netd_sel_segment *ss = 0;
    SOCK_SEL_MAP(seg, &ss);
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, ss, 1);
    
    assert(ss->sel_op[probe].sync != NETD_SEL_SYNC_CLOSE);
    
    if (!atomic_compare_exchange((atomic_t *)&ss->sel_op[probe].init, 0, 1)) {
	struct select_worker_arg *args = 
	    (struct select_worker_arg *) malloc(sizeof(*args));
	if (!args)
	    return -E_NO_MEM;
	    
	args->seg = seg;
	args->op = probe;
	
	struct cobj_ref tobj;
	int r = thread_create(start_env->proc_container, sock_statsync_worker, 
			      args, &tobj, "select thread");	
	if (r < 0) {
	    free(args);
	    return r;
	}
    }

    atomic_compare_exchange((atomic_t *)&ss->sel_op[probe].sync,
			    NETD_SEL_SYNC_DONE,
			    NETD_SEL_SYNC_REQUEST);
    sys_sync_wakeup(&ss->sel_op[probe].sync);
    
    return 0;
}

static int
sock_statsync_cb1(void *arg0, void *arg1, dev_probe_t probe)
{
    struct cobj_ref *seg = (struct cobj_ref *)arg0;

    struct netd_sel_segment *ss = 0;
    SOCK_SEL_MAP(*seg, &ss);
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, ss, 1);

    atomic_compare_exchange((atomic_t *)&ss->sel_op[probe].sync,
			    NETD_SEL_SYNC_REQUEST,
			    NETD_SEL_SYNC_DONE);
    sys_sync_wakeup(&ss->sel_op[probe].sync);
    
    return 0;
}

extern "C" void
netd_free_sel_seg(struct cobj_ref *seg)
{
    struct netd_sel_segment *ss = 0;

    int r = segment_map(*seg, 0, SEGMAP_READ | SEGMAP_WRITE, 
			(void **)&ss, 0, 0);
    if (r < 0) {
	cprintf("netd_free_sel_seg: unable to map seg: %s", e2s(r));
    } else {
	scope_guard2<int, void*, int> unmap(segment_unmap_delayed, ss, 1);
	for (int i = 0; i < netd_sel_op_count; i++) {
	    ss->sel_op[i].sync = NETD_SEL_SYNC_CLOSE;
	    sys_sync_wakeup(&ss->sel_op[i].sync);
	}
    }
    debug_print(dbg, "unrefing %ld.%ld", seg->container, seg->object);
    sys_obj_unref(*seg);
    memset(seg, sizeof(*seg), 0);
}


extern "C" int
netd_new_sel_seg(uint64_t ct, int sock, struct ulabel *l, struct cobj_ref *seg)
{
    struct netd_sel_segment *ss = 0;

    label user_taint;
    user_taint.copy_from(l);

    label thread_taint;
    thread_cur_label(&thread_taint);
    thread_taint.transform(label::star_to, 1);
    
    label seg_taint;
    user_taint.merge(&thread_taint, &seg_taint, label::max, label::leq_starlo);
    
    int r;
    if ((r = segment_alloc(ct, 
			   sizeof(struct netd_sel_segment), seg, (void **)&ss, 
			   seg_taint.to_ulabel(), "select seg")) < 0)
	return r;
    
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, ss, 1);
    
    debug_print(dbg, "select segment %ld.%ld", seg->container, seg->object);
    debug_print(dbg, "select segment label %s", seg_taint.to_string());

    memset(ss, 0, sizeof(*ss));
    ss->sock = sock;
    
    return 0;
}

extern "C" int
netd_wstat(struct cobj_ref *seg, dev_probe_t probe, struct wait_stat *wstat)
{
    static uint64_t offsets[2] = 
	{ offsetof(struct netd_sel_segment, sel_op[0].gen),
	  offsetof(struct netd_sel_segment, sel_op[1].gen) };
    
    struct netd_sel_segment *ss = 0;
    SOCK_SEL_MAP(*seg, &ss);
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, ss, 1);

    WS_SETOBJ(wstat, *seg, offsets[probe]);
    WS_SETVAL(wstat, ss->sel_op[probe].gen);
    WS_SETCBARG(wstat, seg);
    WS_SETCB0(wstat, &sock_statsync_cb0);
    WS_SETCB1(wstat, &sock_statsync_cb1);

    return 0;
}
