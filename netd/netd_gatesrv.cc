extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/declassify.h>
#include <inc/setjmp.h>
#include <inc/atomic.h>
#include <inc/utrap.h>
#include <netd/netdipc.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>
#include <netd/netdsrv.hh>

static uint64_t netd_server_enabled;
static struct cobj_ref declassify_gate;
static struct cobj_ref netd_asref;

#ifdef JOS_ARCH_arm 
#include <pthread.h>
#include <pkg/htcdream/smdd/msm_rpcrouter2.h>
#include <inc/smdd.h>
#include <pkg/htcdream/support/smddgate.h>
#include <pkg/htcdream/smdd/smd_rmnet.h>
static uint64_t last_radio_nsec;
#endif

// check when we last sent or received. if it's been a while
// (>= 20 seconds), then the radio will have powered itself
// down and our operation will cost a lot more than usual.
//
// if we send a packet while not idle, we effectively push the
// next idling period ahead 20 sec, which we should be paying
// for
//
// so, figure out how much further we pushed it ahead and bill
// appropriately. operations that are back-to-back won't get
// punished badly. those further away will, and rightly so.

// radio on/off cycle calculated to be about 9J by experiment
// we'll bill threads (9J/20) * diff_sec if the radio is on,
// so they pay for extending the on period
// if it's not on, we'll require 125% of 9J to fire it up.
// this will leave .25 * 9J in netd's bucket so that the
// initiating thread doesn't just power it up and immediately
// run out of energy 

#define RADIO_ON_COST		UINT64(9 * 1000 * 1000)		/* 9e6uJ */
#define RADIO_EXTEND_SEC_COST	UINT64(450 * 1000)		/* 450,000 uJ */
#define NETD_COOP_BUFFER	0.25

static int64_t
get_radio_idle_cost()
{
#ifdef JOS_ARCH_arm
	uint64_t now_nsec = sys_clock_nsec();
	uint64_t diff_sec = (now_nsec - last_radio_nsec) / UINT64(1000000000);

	if (diff_sec > 20)
		return RADIO_ON_COST;
	else if (diff_sec != 0)
		return RADIO_EXTEND_SEC_COST * diff_sec;
#endif

	return 0;
}

// cooperative energy sharing to fire the radio up
// if an app doesn't have enough energy to turn the radio on, or even if
// it is on but has been idle long enough that they'll need to make up
// the difference for extending the sleep timeout, we'll store the thread's
// energy into the communal bucket and make progress whenever we can.
static cobj_ref netd_coop_rsobj;
static pthread_mutex_t netd_coop_mutex;

static void
bill_reserve()
{
	cobj_ref th_rsobj;
	int64_t cost;

	int r = sys_self_get_active_reserve(&th_rsobj);
	assert(r == 0);

	while ((cost = get_radio_idle_cost()) != 0) {
		pthread_mutex_lock(&netd_coop_mutex);

		// require more energy if the radio is going off->on,
		// since we don't want to block immediately after powering up
		int64_t needed;
		if (cost == RADIO_ON_COST)
			needed = (double)cost * (1.0 + NETD_COOP_BUFFER);
		else
			needed = cost;

		int64_t netd_coop_level = sys_reserve_get_level(netd_coop_rsobj);
		assert(netd_coop_level >= 0);

		if (netd_coop_level >= needed) {
			// transfer cost from netd to our cap, burn it, and run
			sys_reserve_transfer(netd_coop_rsobj, th_rsobj,
			    cost, 0);
			pthread_mutex_unlock(&netd_coop_mutex);
			sys_self_bill(THREAD_BILL_ENERGY_RAW, cost);
			break;
		}

		int64_t th_level = sys_reserve_get_level(th_rsobj);
		if ((th_level + netd_coop_level) >= needed) {
			int64_t transfer;

			// take only the difference from netd
			// be nice and leave a penny for whoever else comes
			if (th_level < cost)
				transfer = cost - th_level;
			else
				transfer = 0;

			// burn netd's, burn thread's, and run 
			sys_reserve_transfer(netd_coop_rsobj, th_rsobj,
			    transfer, 0);
			pthread_mutex_unlock(&netd_coop_mutex);
			sys_self_bill(THREAD_BILL_ENERGY_RAW, cost);
			break;
		}

		// dump whatever evergy we have into net's coop bucket
		// this should yield us, but we'll be back the next quantum
		// to try again.
		int64_t r = sys_reserve_transfer(th_rsobj, netd_coop_rsobj, th_level, 0);
		assert(r >= 0);
		pthread_mutex_unlock(&netd_coop_mutex);
		usleep(100000);
	}
}

static void
netd_bill_energy_pre(const struct netd_op_args *netd_op)
{
    uint32_t count = 0;
    int bill = 0;

    switch (netd_op->op_type) {
    case netd_op_socket:		break;
    case netd_op_bind:			break;
    case netd_op_listen:		break;
    case netd_op_accept:		break;

    case netd_op_connect:
	count = 60 * 3;		// syn, syn/ack, ack
	bill = 1;
	break;

    case netd_op_close:
	count = 60;		// fin
	bill = 1;
	break;

    case netd_op_getsockname:		break;
    case netd_op_getpeername:		break;
    case netd_op_setsockopt:		break;
    case netd_op_getsockopt:		break;

    case netd_op_send:
	count = netd_op->send.count;
	bill = 1;
	break;

    case netd_op_sendto:
	count = netd_op->sendto.count;
	bill = 1;
	break;

    case netd_op_recvfrom:		break;	// in netd_bill_energy_post()
    case netd_op_notify:		break;
    case netd_op_probe:			break;
    case netd_op_statsync:		break;
    case netd_op_shutdown:		break;
    case netd_op_ioctl:			break;

    default:
	return;
    }

    // user could do an arbitrarily large send, but they just
    // screw themselves over.

    (void)count;

    if (bill)
        bill_reserve();
}

static void
netd_bill_energy_post(const struct netd_op_args *netd_op)
{
    uint32_t count = 0;
    int bill = 0;

    switch (netd_op->op_type) {
    case netd_op_socket:		break;
    case netd_op_bind:			break;
    case netd_op_listen:		break;

    case netd_op_accept:
	count = 60 * 3;		// syn, syn/ack, ack
	bill = 1;
	break;

    case netd_op_connect:		break;
    case netd_op_close:			break;
    case netd_op_getsockname:		break;
    case netd_op_getpeername:		break;
    case netd_op_setsockopt:		break;
    case netd_op_getsockopt:		break;
    case netd_op_send:			break;
    case netd_op_sendto:		break;

    case netd_op_recvfrom:
	count = (netd_op->rval >= 0) ? netd_op->rval : 0;
	bill = 1;
	break;

    case netd_op_notify:		break;
    case netd_op_probe:			break;
    case netd_op_statsync:		break;
    case netd_op_shutdown:		break;
    case netd_op_ioctl:			break;

    default:
	return;
    }

    (void)count;

    if (bill)
	bill_reserve();
}

static void __attribute__((noreturn))
netd_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    netd_handler h = (netd_handler) (uintptr_t) a;
    
    while (!netd_server_enabled)
	sys_sync_wait(&netd_server_enabled, 0, UINT64(~0));

    uint64_t netd_ct = start_env->proc_container;
    struct cobj_ref arg = gcd->param_obj;

    int64_t arg_copy_id = sys_segment_copy(arg, netd_ct, 0,
					   "netd_gate_entry() args");
    if (arg_copy_id < 0)
	panic("netd_gate_entry: cannot copy <%"PRIu64".%"PRIu64"> args: %s",
	      arg.container, arg.object, e2s(arg_copy_id));
    sys_obj_unref(arg);

    struct cobj_ref arg_copy = COBJ(netd_ct, arg_copy_id);
    struct netd_op_args *netd_op = 0;
    int r = segment_map(arg_copy, 0, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_bill_energy_pre(netd_op);
    h(netd_op);
    netd_bill_energy_post(netd_op);
    segment_unmap(netd_op);

    uint64_t copy_back_ct = gcd->taint_container;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct, 0,
					    "netd_gate_entry reply");
    if (copy_back_id < 0)
	panic("netd_gate_entry: cannot copy back: %s", e2s(copy_back_id));

    sys_obj_unref(arg_copy);
    gcd->param_obj = COBJ(copy_back_ct, copy_back_id);
    gcd->declassify_gate = declassify_gate;
    rg->ret(0, 0, 0);
}

static void
netd_ipc_setup(uint64_t taint_ct, struct cobj_ref ipc_seg, uint64_t flags, 
	       void **va, uint64_t *bytes, struct cobj_ref *temp_as)
{
    uint64_t netd_ct = start_env->proc_container;

    error_check(sys_self_addref(netd_ct));
    scope_guard<int, cobj_ref> unref(sys_obj_unref, COBJ(netd_ct, thread_id()));
    
    // Create private container backed by user resources + AS clone
    {
	label private_label;
	thread_cur_label(&private_label);
	private_label.transform(label::star_to, private_label.get_default());
	private_label.set(start_env->process_grant, 0);
	private_label.set(start_env->process_taint, 3);
	
	int64_t private_ct;
	error_check(private_ct =
		    sys_container_alloc(taint_ct, private_label.to_ulabel(),
					"netd_fast private", 0, CT_QUOTA_INF));
	
	int64_t asid;
	error_check(asid = sys_as_copy(netd_asref, private_ct,
				       0, "netd_ipc temp AS"));
	*temp_as = COBJ(private_ct, asid);
    }
    
    error_check(sys_self_set_as(*temp_as));
    segment_as_switched();
    error_check(segment_map(ipc_seg, 0, flags, va, bytes, 0));
}

static void __attribute__((noreturn))
netd_fast_gate_entry(uint64_t a, struct gate_call_data *gcd, gatesrv_return *rg)
{
    netd_handler h = (netd_handler) (uintptr_t) a;
    uint64_t netd_ct = start_env->proc_container;
    struct cobj_ref temp_as;
    struct netd_ipc_segment *ipc = 0;
    uint64_t map_bytes = 0;

    error_check(sys_segment_resize(COBJ(0, kobject_id_thread_sg), 2 * PGSIZE));

    while (!netd_server_enabled)
	sys_sync_wait(&netd_server_enabled, 0, UINT64(~0));

    netd_ipc_setup(gcd->taint_container, gcd->param_obj, 
		   SEGMAP_READ | SEGMAP_WRITE,
		   (void **) &ipc, &map_bytes, &temp_as);

    struct jos_jmp_buf pgfault;
    if (jos_setjmp(&pgfault) != 0)
	thread_halt();
    tls_data->tls_pgfault = &pgfault;

    if (map_bytes != sizeof(*ipc))
	throw basic_exception("wrong size IPC segment: %"PRIu64" should be %"PRIu64"\n",
			      map_bytes, (uint64_t) sizeof(*ipc));

    for (;;) {
	while (ipc->sync == NETD_IPC_SYNC_REPLY)
	    sys_sync_wait(&ipc->sync, NETD_IPC_SYNC_REPLY, UINT64(~0));

	error_check(sys_self_set_as(netd_asref));
	error_check(sys_self_addref(netd_ct));
	scope_guard<int, cobj_ref> unref(sys_obj_unref,
					 COBJ(netd_ct, thread_id()));
	segment_as_switched();

	// Map shared memory segment & execute operation
	{
	    struct netd_ipc_segment *ipc_shared = 0;
	    error_check(segment_map(gcd->param_obj, 0,
				    SEGMAP_READ | SEGMAP_WRITE
						| SEGMAP_VECTOR_PF,
				    (void **) &ipc_shared, &map_bytes, 0));
	    scope_guard<int, void *> unmap(segment_unmap, ipc_shared);

	    struct netd_ipc_segment *ipc_copy;
	    ipc_copy = (struct netd_ipc_segment *) malloc(sizeof(*ipc_copy));
	    if (!ipc_copy)
		throw basic_exception("cannot allocate ipc_copy");
	    scope_guard<void, void*> free_copy(free, ipc_copy);

	    struct jos_jmp_buf pgfault2;
	    if (jos_setjmp(&pgfault2) != 0) {
		tls_data->tls_pgfault = &pgfault;
		break;
	    }
	    tls_data->tls_pgfault = &pgfault2;

	    while (ipc_shared->sync == NETD_IPC_SYNC_REQUEST) {
		memcpy(&ipc_copy->args, &ipc_shared->args,
		       ipc_shared->args.size);
		netd_bill_energy_pre(&ipc_copy->args);
		h(&ipc_copy->args);
		netd_bill_energy_post(&ipc_copy->args);
		memcpy(&ipc_shared->args, &ipc_copy->args,
		       ipc_copy->args.size);

		ipc_shared->sync = NETD_IPC_SYNC_REPLY;
		error_check(sys_sync_wakeup(&ipc_shared->sync));

		int64_t nsec_keepalive = sys_clock_nsec() + NSEC_PER_SECOND;
		while (ipc_shared->sync == NETD_IPC_SYNC_REPLY &&
		       sys_clock_nsec() < nsec_keepalive)
		    sys_sync_wait(&ipc_shared->sync, NETD_IPC_SYNC_REPLY,
				  nsec_keepalive);
	    }

	    tls_data->tls_pgfault = &pgfault;
	}

	unref.force();
	error_check(sys_self_set_as(temp_as));
    }

    thread_halt();
}

static void
netd_gate_init(uint64_t gate_ct, label *l, label *clear, netd_handler h)
{
    try {
	label verify(3);

	gatesrv_descriptor gd;
	gd.gate_container_ = gate_ct;
	gd.label_ = l;
	gd.clearance_ = clear;
	gd.verify_ = &verify;
	gd.arg_ = (uintptr_t)h;

	gd.name_ = "netd-lwip";
	gd.func_ = &netd_gate_entry;
	gate_create(&gd);

	gd.name_ = "netd-lwip-fast";
	gd.func_ = &netd_fast_gate_entry;
	gd.flags_ = GATESRV_NO_THREAD_ADDREF | GATESRV_KEEP_TLS_STACK;
	gate_create(&gd);
    } catch (error &e) {
	cprintf("netd_server_init: %s\n", e.what());
	throw;
    } catch (std::exception &e) {
	cprintf("netd_server_init: %s\n", e.what());
	throw;
    }    
    return;
}

void
netd_server_init(uint64_t gate_ct,
		 uint64_t taint_handle,
		 label *l, label *clear, 
		 netd_handler h)
{
    error_check(sys_self_get_as(&netd_asref));

    declassify_gate =
	gate_create(start_env->shared_container, "declassifier",
		    0, 0, 0, &declassifier, taint_handle);

    netd_gate_init(gate_ct, l, clear, h);
}

#ifdef JOS_ARCH_arm
static void *
radio_active_poll(void *unused)
{
	// update local cache of last radio time
	while (1) {
		struct rmnet_stats rs;
		uint64_t now_nsec = sys_clock_nsec();

		int r = smddgate_rmnet_stats(0, &rs);
		assert(r == 0);

		last_radio_nsec = JMAX(rs.tx_last_nsec, rs.rx_last_nsec);
		sleep(1);

		// shut up gcc attribute candidate shit
		if (now_nsec == ~UINT64(0)) break;
	}

	return NULL;
}
#endif

void
netd_server_enable(void)
{
    netd_server_enabled = 1;

    // init the cooperative reserve junk
    pthread_mutex_init(&netd_coop_mutex, NULL);

    label l(1);
    uint64_t ctid = start_env->shared_container;	
    int64_t rsid = sys_reserve_create(ctid, l.to_ulabel(), "netd coop reserve");
    assert(rsid >= 0);
    netd_coop_rsobj = COBJ(ctid, rsid); 


    // we poll rmnet to get the last tx and rx times...
#ifdef JOS_ARCH_arm
    pthread_t tid;
    pthread_create(&tid, NULL, radio_active_poll, NULL);
#endif

    sys_sync_wakeup(&netd_server_enabled);
}
