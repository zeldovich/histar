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

static struct cobj_ref netd_gate;
static int tainted;

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

int
netd_call(struct cobj_ref gate, struct netd_op_args *a)
{
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
	// XXX this isn't smart enough to figure out if we're already
	// tainted or not.  this makes it rather annoying for running
	// stuff over telnetd.

	// process_report_taint();
	tainted = 1;
    }

    try {
	struct gate_call_data gcd;
	gcd.param_obj = seg;
	gate_call(gate, &gcd, 0, 0, 0);
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
