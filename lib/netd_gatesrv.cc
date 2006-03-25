extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/declassify.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>

static uint64_t netd_taint_handle;
enum { netd_do_taint = 1 };

static void __attribute__((noreturn))
netd_gate_entry(void *x, struct gate_call_data *gcd, gatesrv_return *rg)
{
    uint64_t netd_ct = (uint64_t) x;
    struct cobj_ref arg = gcd->param_obj;

    int64_t arg_copy_id = sys_segment_copy(arg, netd_ct, 0,
					   "netd_gate_entry() args");
    if (arg_copy_id < 0)
	panic("netd_gate_entry: cannot copy <%ld.%ld> args: %s",
	      arg.container, arg.object, e2s(arg_copy_id));
    sys_obj_unref(arg);

    struct cobj_ref arg_copy = COBJ(netd_ct, arg_copy_id);
    struct netd_op_args *netd_op = 0;
    int r = segment_map(arg_copy, SEGMAP_READ | SEGMAP_WRITE, (void**)&netd_op, 0);
    if (r < 0)
	panic("netd_gate_entry: cannot map args: %s\n", e2s(r));

    netd_dispatch(netd_op);
    segment_unmap(netd_op);

    label args_label(1);

    if (netd_do_taint)
	args_label.set(netd_taint_handle, 2);

    uint64_t copy_back_ct = gcd->taint_container;
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
    if (netd_do_taint) {
	// XXX
	// having gatesrv as an object seems like a bad idea.
	// we really want to encapsulate all of the state into the gate,
	// so that we don't leak memory as we create more and more gates..
	label tl, tc;

	thread_cur_label(&tl);
	thread_cur_clearance(&tc);

	gatesrv *g = new gatesrv(gcd->taint_container, "declassifier",
				 &tl, &tc);
	g->set_entry_container(start_env->proc_container);
	g->set_entry_function(&declassifier, 0);
	g->enable();
	gcd->declassify_gate = g->gate();

	cs->set(netd_taint_handle, 2);
    }

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
