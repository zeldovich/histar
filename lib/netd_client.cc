extern "C" {
#include <inc/memlayout.h>
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <string.h>
}

#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/gateparam.hh>
#include <inc/error.hh>

static int netd_client_inited;
static struct cobj_ref netd_gate;
static int tainted;

static int
netd_client_init(void)
{
    int64_t netd_ct = container_find(start_env->root_container,
				     kobj_container, "netd");
    if (netd_ct < 0)
	return netd_ct;

    int64_t gate_id = container_find(netd_ct, kobj_gate, "netd");
    if (gate_id < 0)
	return gate_id;

    netd_client_inited = 1;
    netd_gate = COBJ(netd_ct, gate_id);
    return 0;
}

int
netd_call(struct netd_op_args *a) {
    for (int i = 0; i < 10 && netd_client_inited == 0; i++) {
	int r = netd_client_init();
	if (r < 0)
	    thread_sleep(100);
    }

    if (netd_client_inited == 0) {
	cprintf("netd_call: cannot initialize netd client\n");
	return -1;
    }

    struct ulabel *seg_label = label_get_current();
    if (seg_label == 0) {
	cprintf("netd_call: cannot get label\n");
	return -E_NO_MEM;
    }
    label_change_star(seg_label, seg_label->ul_default);

    struct cobj_ref seg;
    void *va = 0;
    int r = segment_alloc(kobject_id_thread_ct, PGSIZE, &seg, &va,
			  seg_label, "netd_call() args");
    label_free(seg_label);
    if (r < 0)
	return r;

    memcpy(va, a, sizeof(*a));
    segment_unmap(va);

    if (tainted == 0) {
	process_report_taint();
	tainted = 1;
    }

    try {
	struct gate_call_data gcd;
	gcd.param_obj = seg;
	gate_call(netd_gate, &gcd, 0, 0, 0);
	seg = gcd.param_obj;
    } catch (error &e) {
	cprintf("netd_call: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("netd_call: %s\n", e.what());
	return -1;
    }

    va = 0;
    r = segment_map(seg, SEGMAP_READ | SEGMAP_WRITE, &va, 0);
    if (r < 0) {
	cprintf("netd_call: cannot map returned segment: %s\n", e2s(r));
	return r;
    }

    memcpy(a, va, sizeof(*a));
    int rval = a->rval;

    segment_unmap(va);
    sys_obj_unref(seg);
    return rval;
}
