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

static int netd_server_enabled;
static struct cobj_ref declassify_gate;

static void __attribute__((noreturn))
netd_gate_entry(void *x, struct gate_call_data *gcd, gatesrv_return *rg)
{
    while (!netd_server_enabled)
	sys_self_yield();

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

struct cobj_ref
netd_server_init(uint64_t gate_ct,
		 uint64_t taint_handle,
		 label *l, label *clear)
{
    label cur_l, cur_c;
    thread_cur_label(&cur_l);
    thread_cur_clearance(&cur_c);

    declassify_gate =
	gate_create(start_env->shared_container, "declassifier",
		    &cur_l, &cur_c,
		    &declassifier, (void *) taint_handle);

    try {
	uint64_t entry_ct = start_env->proc_container;
	return gate_create(gate_ct, "netd", l, clear,
			   &netd_gate_entry, (void *) entry_ct);
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
}
