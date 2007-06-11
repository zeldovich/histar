extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <netd/netdlwip.h>
#include <netd/netdlinux.h>
#include <netd/netdipc.h>

#include <string.h>
#include <errno.h>
}

#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>
#include <netd/netdclnt.hh>

enum { netd_lwip_mode = 1, netd_linux_mode = 2 };
static char netd_mode;
static struct cobj_ref netd_gate, netd_fast_gate;

static int
netd_client_init(void)
{
    for (int i = 0; i < 10; i++) {
	if (netd_lwip_client_init(&netd_gate, &netd_fast_gate) == 0) {
	    netd_mode = netd_lwip_mode;
	    return 0;
	} else if (netd_linux_client_init(&netd_gate) == 0) {
	    netd_mode = netd_linux_mode;
	    return 0;
	}
	thread_sleep_nsec(NSEC_PER_SECOND / 10);
    }
    cprintf("netd_client_init: unable to find a netd\n");
    return -E_NOT_FOUND;
}

struct cobj_ref
netd_get_gate(void)
{
    if (!netd_mode) {
	int r = netd_client_init();
	if (r < 0) {
	    cprintf("netd_get_gate: cannot initialize netd client\n");
	    return COBJ(0, 0);
	}
    }
    return netd_gate;
}    

struct cobj_ref
netd_get_fast_gate(void)
{
    if (!netd_mode) {
	int r = netd_client_init();
	if (r < 0) {
	    cprintf("netd_get_fast_gate: cannot initialize netd client\n");
	    return COBJ(0, 0);
	}
    }
    return netd_fast_gate;
}    

// Fast netd IPC support
enum { max_fipc = 256 };
static jthread_mutex_t fipc_handle_mu;
static int64_t fipc_taint, fipc_grant;
static struct netd_fast_ipc_state fipc[max_fipc];

static void
netd_fast_worker(void *arg)
{
    struct netd_fast_ipc_state *s = (struct netd_fast_ipc_state *) arg;

    try {
	label taint_l(1);
	taint_l.set(fipc_grant, 0);
	taint_l.set(fipc_taint, 3);

	label th_l, shared_l;
	thread_cur_label(&th_l);
	th_l.transform(label::star_to, 1);
	taint_l.merge(&th_l, &shared_l, label::max, label::leq_starlo);

	cobj_ref shared_seg;
	error_check(segment_alloc(s->fast_ipc_gatecall->call_ct(),
				  sizeof(*(s->fast_ipc)),
				  &shared_seg, 0, shared_l.to_ulabel(), 
				  "netd fast IPC segment"));

	error_check(sys_obj_set_fixedquota(shared_seg));
	error_check(sys_segment_addref(shared_seg, start_env->proc_container));
	error_check(segment_map(COBJ(start_env->proc_container, shared_seg.object),
				0, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &s->fast_ipc, 0, 0));

	gate_call_data gcd;
	gcd.param_obj = shared_seg;

	s->fast_ipc_inited_shared_ct = start_env->shared_container;
	s->fast_ipc_inited = 2;
	sys_sync_wakeup(&s->fast_ipc_inited);

	s->fast_ipc_gatecall->call(&gcd);
    } catch (std::exception &e) {
	cprintf("netd_fast_worker: %s\n", e.what());
    }

    cprintf("netd_fast_worker: returning\n");
    s->fast_ipc_inited = 0;
}

static void
netd_fast_init_global(void)
{
    if (fipc_taint <= 0 || fipc_grant <= 0) {
	scoped_jthread_lock l(&fipc_handle_mu);
	if (fipc_taint <= 0)
	    error_check(fipc_taint = handle_alloc());
	if (fipc_grant <= 0)
	    error_check(fipc_grant = handle_alloc());
    }
}

static void
netd_fast_init(struct netd_fast_ipc_state *s)
{
    for (;;) {
	if (s->fast_ipc_inited == 2 &&
	    s->fast_ipc_inited_shared_ct != start_env->shared_container)
	{
	    if (s->fast_ipc) {
		segment_unmap(s->fast_ipc);
		s->fast_ipc = 0;
	    }

	    s->fast_ipc_inited = 0;
	}

	if (s->fast_ipc_inited == 0) {
	    label ds(3);
	    ds.set(fipc_grant, LB_LEVEL_STAR);
	    ds.set(fipc_taint, LB_LEVEL_STAR);

	    label dr(0);
	    dr.set(fipc_taint, 3);

	    s->fast_ipc_gate = netd_get_fast_gate();
	    if (s->fast_ipc_gate.object == 0)
		throw basic_exception("netd_fast_init: mode %d does not support fast-ipc", netd_mode);
	    s->fast_ipc_gatecall = new gate_call(s->fast_ipc_gate, 0, &ds, &dr);

	    s->fast_ipc_inited = 1;
	    cobj_ref fast_ipc_th;
	    error_check(thread_create(start_env->proc_container,
				      &netd_fast_worker, s,
				      &fast_ipc_th, "netd fast ipc"));
	}

	if (s->fast_ipc_inited == 2)
	    return;

	sys_sync_wait(&s->fast_ipc_inited, 1, UINT64(~0));
    }
}

static void
netd_fast_call(struct netd_fast_ipc_state *s, struct netd_op_args *a)
{
    memcpy(&s->fast_ipc->args, a, a->size);
    s->fast_ipc->sync = NETD_IPC_SYNC_REQUEST;
    sys_sync_wakeup(&s->fast_ipc->sync);

    while (s->fast_ipc->sync != NETD_IPC_SYNC_REPLY)
	sys_sync_wait(&s->fast_ipc->sync, NETD_IPC_SYNC_REQUEST, UINT64(~0));
    memcpy(a, &s->fast_ipc->args, s->fast_ipc->args.size);

}

void
netd_fast_call(struct netd_op_args *a)
{
    netd_fast_init_global();

    for (;;) {
	for (int i = 0; i < max_fipc; i++) {
	    struct netd_fast_ipc_state *s = &fipc[i];
	    scoped_jthread_trylock l(&s->fast_ipc_mu);
	    if (l.acquired()) {
		netd_fast_init(s);
		netd_fast_call(s, a);
		return;
	    }
	}

	cprintf("netd_fast_call: out of fipc slots?!\n");
    }
}

int 
netd_slow_call(struct cobj_ref gate, struct netd_op_args *a)
{
    try {
	gate_call c(gate, 0, 0, 0);
	
	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(c.call_ct(), sizeof(*a), &seg, &va,
				  0, "netd_call() args"));
	memcpy(va, a, sizeof(*a));
	segment_unmap(va);
	
	struct gate_call_data gcd;
	gcd.param_obj = seg;
	c.call(&gcd);
	
	va = 0;
	error_check(segment_map(gcd.param_obj, 0, SEGMAP_READ, &va, 0, 0));
	memcpy(a, va, sizeof(*a));
	segment_unmap(va);
    } catch (error &e) {
	cprintf("netd_slow_call: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("netd_slow_call: %s\n", e.what());
	return -1;
    }

    if (a->rval < 0)
	errno = a->rerrno;
    return a->rval;
}

int
netd_call(struct Fd *fd, struct netd_op_args *a)
{
    int r;
    if (!netd_mode) {
	r = netd_client_init();
	if (r < 0)
	    return r;
    }

    if (netd_mode == netd_lwip_mode)
	return netd_lwip_call(fd, a);
    else if(netd_mode == netd_linux_mode)
	return netd_linux_call(fd, a);

    cprintf("netd_call: bad mode: %d\n", netd_mode);
    return -E_UNSPEC;
}
