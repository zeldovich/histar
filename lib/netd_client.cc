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

int
netd_call(struct cobj_ref gate, struct netd_op_args *a)
{
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

    return a->rval;
}
