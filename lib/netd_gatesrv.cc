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

#include <stdlib.h>
#include <string.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/scopeguard.hh>

static uint64_t netd_server_enabled;
static struct cobj_ref declassify_gate;
static struct cobj_ref netd_asref;

static void __attribute__((noreturn))
netd_gate_entry(void *x, struct gate_call_data *gcd, gatesrv_return *rg)
{
    while (!netd_server_enabled)
	sys_sync_wait(&netd_server_enabled, 0, ~0UL);

    uint64_t netd_ct = start_env->proc_container;
    struct cobj_ref arg = gcd->param_obj;

    int64_t arg_copy_id = sys_segment_copy(arg, netd_ct, 0,
					   "netd_gate_entry() args");
    if (arg_copy_id < 0)
	panic("netd_gate_entry: cannot copy <%ld.%ld> args: %s",
	      arg.container, arg.object, e2s(arg_copy_id));
    sys_obj_unref(arg);

    struct cobj_ref arg_copy = COBJ(netd_ct, arg_copy_id);
    struct netd_op_args *netd_op = 0;
    int r = segment_map(arg_copy, 0, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_dispatch(netd_op);
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

static void __attribute__((noreturn))
netd_fast_gate_entry(void *x, struct gate_call_data *gcd, gatesrv_return *rg)
{
    uint64_t netd_ct = start_env->proc_container;
    struct cobj_ref temp_as;
    struct netd_ipc_segment *ipc = 0;
    uint64_t map_bytes = 0;

    while (!netd_server_enabled)
	sys_sync_wait(&netd_server_enabled, 0, ~0UL);

    // Scope to force object destructors
    {
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
		sys_container_alloc(gcd->taint_container,
				    private_label.to_ulabel(),
				    "netd_fast private",
				    0, CT_QUOTA_INF));

	    struct u_address_space uas;
	    uas.size = 64;
	    uas.ents = (struct u_segment_mapping *) malloc(sizeof(*uas.ents) * uas.size);
	    if (!uas.ents)
		throw error(-E_NO_MEM, "netd_fast_gate_entry: out of memory");

	    int64_t asid;
	    error_check(asid = sys_as_create(private_ct, 0, "netd_fast temp AS"));
	    temp_as = COBJ(private_ct, asid);
	    error_check(sys_as_get(netd_asref, &uas));
	    error_check(sys_as_set(temp_as, &uas));
	}

	error_check(sys_self_set_as(temp_as));
	segment_as_switched();

	error_check(segment_map(gcd->param_obj, 0, SEGMAP_READ | SEGMAP_WRITE,
				(void **) &ipc, &map_bytes, 0));
	if (map_bytes != sizeof(*ipc))
	    throw basic_exception("wrong size IPC segment: %ld should be %ld\n",
				  map_bytes, sizeof(*ipc));
    }

    for (;;) {
	while (ipc->sync == NETD_IPC_SYNC_REPLY)
	    sys_sync_wait(&ipc->sync, NETD_IPC_SYNC_REPLY, ~0UL);

	error_check(sys_self_addref(netd_ct));
	scope_guard<int, cobj_ref> unref(sys_obj_unref, COBJ(netd_ct, thread_id()));

	error_check(sys_self_set_as(netd_asref));
	segment_as_switched();

	// Map shared memory segment & execute operation
	{
	    struct netd_ipc_segment *ipc_shared = 0;
	    error_check(segment_map(gcd->param_obj, 0, SEGMAP_READ | SEGMAP_WRITE | SEGMAP_VECTOR_PF,
				    (void **) &ipc_shared, &map_bytes, 0));
	    scope_guard<int, void *> unmap(segment_unmap, ipc_shared);

	    cobj_ref copy_seg;
	    struct netd_ipc_segment *ipc_copy = 0;
	    error_check(segment_alloc(start_env->proc_container, sizeof(*ipc_copy),
				      &copy_seg, (void **) &ipc_copy, 0, "ipc copy"));
	    scope_guard<int, void *> unmap2(segment_unmap, ipc_copy);
	    scope_guard<int, cobj_ref> drop(sys_obj_unref, copy_seg);

	    while (ipc_shared->sync == NETD_IPC_SYNC_REQUEST) {
		struct jos_jmp_buf pgfault;
		if (jos_setjmp(&pgfault) != 0)
		    break;
		*tls_pgfault = &pgfault;

		memcpy(&ipc_copy->args, &ipc_shared->args, ipc_shared->args.size);
		netd_dispatch(&ipc_copy->args);
		memcpy(&ipc_shared->args, &ipc_copy->args, ipc_copy->args.size);

		ipc_shared->sync = NETD_IPC_SYNC_REPLY;
		error_check(sys_sync_wakeup(&ipc_shared->sync));

		int64_t msec_keepalive = sys_clock_msec() + 1000;
		while (ipc_shared->sync == NETD_IPC_SYNC_REPLY &&
		       sys_clock_msec() < msec_keepalive)
		    sys_sync_wait(&ipc_shared->sync, NETD_IPC_SYNC_REPLY, msec_keepalive);
	    }
	}

	unref.force();
	error_check(sys_self_set_as(temp_as));
	segment_as_switched();
    }
}

void
netd_server_init(uint64_t gate_ct,
		 uint64_t taint_handle,
		 label *l, label *clear)
{
    label cur_l, cur_c;
    thread_cur_label(&cur_l);
    thread_cur_clearance(&cur_c);

    error_check(sys_self_get_as(&netd_asref));

    declassify_gate =
	gate_create(start_env->shared_container, "declassifier",
		    &cur_l, &cur_c,
		    &declassifier, (void *) taint_handle);

    try {
	gatesrv_descriptor gd;
	gd.gate_container_ = gate_ct;
	gd.label_ = l;
	gd.clearance_ = clear;
	gd.arg_ = 0;

	gd.name_ = "netd";
	gd.func_ = &netd_gate_entry;
	gate_create(&gd);

	gd.name_ = "netd-fast";
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
}

void
netd_server_enable(void)
{
    netd_server_enabled = 1;
    sys_sync_wakeup(&netd_server_enabled);
}
