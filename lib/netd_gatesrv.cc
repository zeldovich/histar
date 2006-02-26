extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
}

#include <inc/gatesrv.hh>
#include <inc/cpplabel.hh>

static void __attribute__((noreturn))
netd_gate_entry(void *x, struct cobj_ref arg, gatesrv_return *rg)
{
    uint64_t netd_ct = (uint64_t) x;

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

    struct ulabel *l = label_get_current();
    if (l == 0)
	panic("cannot allocate label for segment copyback");
    label_change_star(l, l->ul_default);

    uint64_t copy_back_ct = kobject_id_thread_ct;
    int64_t copy_back_id = sys_segment_copy(arg_copy, copy_back_ct,
					    l, "netd_gate_entry() reply");
    if (copy_back_id < 0)
	panic("netd_gate_entry: cannot copy back with label %s: %s",
	      label_to_string(l), e2s(copy_back_id));

    label_free(l);
    sys_obj_unref(arg_copy);

    label *cs = new label(LB_LEVEL_STAR);
    label *ds = new label(3);
    label *dr = new label(0);

    rg->ret(COBJ(copy_back_ct, copy_back_id), cs, ds, dr);
}

gatesrv *
netd_server_init(uint64_t gate_ct, uint64_t entry_ct, label *l, label *clear)
{
    int64_t netd_gate_ct = container_find(gate_ct, kobj_container, "netd gate");
    error_check(netd_gate_ct);

    try {
	gatesrv *g = new gatesrv(netd_gate_ct, "netd", l, clear);
	g->set_entry_container(entry_ct);
	g->set_entry_function(&netd_gate_entry, (void *) entry_ct);
	return g;
    } catch (error &e) {
	printf("netd_server_init: %s\n", e.what());
	//return e.err();
	throw;
    } catch (std::exception &e) {
	printf("netd_server_init: %s\n", e.what());
	//return -1;
	throw;
    }
}
