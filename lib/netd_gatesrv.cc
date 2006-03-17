extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>

static uint64_t netd_taint_handle;

static void __attribute__((noreturn))
netd_gate_entry(void *x, struct gate_call_data *gcd, gatesrv_return *rg)
{
    uint64_t netd_ct = (uint64_t) x;
    struct cobj_ref arg = gcd->param_obj;

    int64_t arg_copy_id = sys_segment_copy(arg, netd_ct,
					   segment_get_default_label(),
					   "netd_gate_entry() args");
    if (arg_copy_id < 0)
	panic("netd_gate_entry: cannot copy args: %s", e2s(arg_copy_id));
    sys_obj_unref(arg);

    struct cobj_ref arg_copy = COBJ(netd_ct, arg_copy_id);
    struct netd_op_args *netd_op = 0;
    int r = segment_map(arg_copy, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_dispatch(netd_op);
    segment_unmap(netd_op);

    label args_label(1);
    args_label.set(netd_taint_handle, 2);

    uint64_t copy_back_ct = kobject_id_thread_ct;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct,
					    args_label.to_ulabel(),
					    "netd_gate_entry reply");
    if (copy_back_id < 0)
	panic("netd_gate_entry: cannot copy back with label %s: %s",
	      args_label.to_string(), e2s(copy_back_id));

    sys_obj_unref(arg_copy);
    gcd->param_obj = COBJ(copy_back_ct, copy_back_id);

    // Contaminate the caller with { taint:2 }
    label *cs = new label(LB_LEVEL_STAR);
    cs->set(netd_taint_handle, 2);

    rg->ret(cs, 0, 0);
}

gatesrv *
netd_server_init(uint64_t gate_ct, uint64_t entry_ct,
		 uint64_t taint_handle,
		 label *l, label *clear)
{
    netd_taint_handle = taint_handle;

    try {
	gatesrv *g = new gatesrv(gate_ct, "netd", l, clear);
	g->set_entry_container(entry_ct);
	g->set_entry_function(&netd_gate_entry, (void *) entry_ct);
	return g;
    } catch (error &e) {
	cprintf("netd_server_init: %s\n", e.what());
	throw;
    } catch (std::exception &e) {
	cprintf("netd_server_init: %s\n", e.what());
	throw;
    }
}
