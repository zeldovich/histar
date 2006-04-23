extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>
#include <inc/labelutil.hh>
#include <inc/pthread.hh>

static struct cobj_ref netd_gate;

static int
netd_client_init(void)
{
    struct fs_inode netd_ct_ino;
    int r = fs_namei("/netd", &netd_ct_ino);
    if (r < 0) {
	cprintf("netd_client_init: fs_namei /netd: %s\n", e2s(r));
	return r;
    }

    uint64_t netd_ct = netd_ct_ino.obj.object;

    int64_t gate_id = container_find(netd_ct, kobj_gate, "netd");
    if (gate_id < 0)
	return gate_id;

    netd_gate = COBJ(netd_ct, gate_id);
    return 0;
}

struct cobj_ref
netd_get_gate(void)
{
    for (int i = 0; i < 10 && netd_gate.object == 0; i++) {
	int r = netd_client_init();
	if (r < 0)
	    thread_sleep(100);
    }

    if (netd_gate.object == 0)
	cprintf("netd_call: cannot initialize netd client\n");

    return netd_gate;
}

void
netd_set_gate(struct cobj_ref g)
{
    netd_gate = g;
}

// Fast netd IPC support
static struct netd_ipc_segment *fast_ipc;
static gate_call *fast_ipc_gatecall;
static cobj_ref fast_ipc_gate;
static uint64_t fast_ipc_inited;
static pthread_mutex_t fast_ipc_mu;

static void
netd_fast_worker(void *arg)
{
    try {
	cobj_ref shared_seg;
	error_check(segment_alloc(fast_ipc_gatecall->call_ct(),
				  sizeof(*fast_ipc),
				  &shared_seg, (void **) &fast_ipc,
				  0, "netd fast IPC segment"));

	gate_call_data gcd;
	gcd.param_obj = shared_seg;

	fast_ipc_inited = 2;
	sys_sync_wakeup(&fast_ipc_inited);

	fast_ipc_gatecall->call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("netd_fast_worker: %s\n", e.what());
    }

    cprintf("netd_fast_worker: returning\n");
    fast_ipc_inited = 0;
}

static void
netd_fast_init(void)
{
    if (fast_ipc_inited == 0) {
	int64_t fast_gate_id = container_find(netd_gate.container,
					      kobj_gate, "netd-fast");
	error_check(fast_gate_id);
	fast_ipc_gate = COBJ(netd_gate.container, fast_gate_id);
	fast_ipc_gatecall = new gate_call(fast_ipc_gate, 0, 0, 0);

	fast_ipc_inited = 1;
	cobj_ref fast_ipc_th;
	error_check(thread_create(start_env->proc_container,
				  &netd_fast_worker, 0,
				  &fast_ipc_th, "netd fast ipc"));
    }
}

static void
netd_fast_call(struct netd_op_args *a)
{
    while (fast_ipc_inited != 2)
	sys_sync_wait(&fast_ipc_inited, 1, ~0UL);

    scoped_pthread_lock l(&fast_ipc_mu);
    memcpy(&fast_ipc->args, a, a->size);
    fast_ipc->sync = NETD_IPC_SYNC_REQUEST;
    sys_sync_wakeup(&fast_ipc->sync);

    while (fast_ipc->sync != NETD_IPC_SYNC_REPLY)
	sys_sync_wait(&fast_ipc->sync, NETD_IPC_SYNC_REQUEST, ~0UL);
    memcpy(a, &fast_ipc->args, fast_ipc->args.size);
}

int
netd_call(struct cobj_ref gate, struct netd_op_args *a)
{
    static int do_fast_calls;

    // A bit of a hack because we need to get tainted first...
    if (do_fast_calls) {
	try {
	    netd_fast_init();
	    netd_fast_call(a);
	    return a->rval;
	} catch (std::exception &e) {
	    cprintf("netd_call: cannot fast-call: %s\n", e.what());
	}
    }

    try {
	gate_call c(gate, 0, 0, 0);

	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(start_env->proc_container, sizeof(*a), &seg, &va,
				  0, "netd_call() args"));
	memcpy(va, a, sizeof(*a));
	segment_unmap(va);

	int64_t copy_ct = c.call_ct();
	int64_t copy_id;
	error_check(copy_id =
	    sys_segment_copy(seg, copy_ct, 0, "netd_call() args copy1"));

	struct gate_call_data gcd;
	gcd.param_obj = COBJ(copy_ct, copy_id);
	c.call(&gcd, 0);

	va = 0;
	error_check(segment_map(gcd.param_obj, SEGMAP_READ, &va, 0));
	memcpy(a, va, sizeof(*a));
	segment_unmap(va);
    } catch (error &e) {
	cprintf("netd_call: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("netd_call: %s\n", e.what());
	return -1;
    }

    if (a->rval >= 0)
	do_fast_calls = 1;

    return a->rval;
}
