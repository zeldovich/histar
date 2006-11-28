extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/ssld.h>
#include <inc/assert.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>

static uint64_t base_ct;
static struct cobj_ref ssld_gate;

static int
ssld_client_init(void)
{
    uint64_t ct = start_env->shared_container;
    int64_t ssld_ct = container_find(ct, kobj_container, "ssld");
    if (ssld_ct < 0)
	return ssld_ct;
    int64_t ssld_gt = container_find(ssld_ct, kobj_gate, "ssld");
    if (ssld_gt < 0)
	return ssld_gt;
    
    ssld_gate = COBJ(ssld_ct, ssld_gt);
    base_ct = ct;
    return 0;
}

struct cobj_ref
ssld_get_gate(void)
{
    if (base_ct != start_env->shared_container) {
	base_ct = 0;
	ssld_gate.object = 0;
    }

    for (int i = 0; i < 10 && ssld_gate.object == 0; i++) {
	int r = ssld_client_init();
	if (r < 0)
	    thread_sleep(100);
    }

    if (ssld_gate.object == 0)
	panic("ssld_get_gate: cannot inialize client -- server inited?");

    return ssld_gate;
}

int
ssld_call(struct cobj_ref gate, struct ssld_op_args *a)
{
    try {
	gate_call c(gate, 0, 0, 0);
	
	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(c.call_ct(), sizeof(*a), &seg, &va,
				  0, "netd_call() args"));
	memcpy(va, a, sizeof(*a));
	segment_unmap(va);
	
	struct gate_call_data gcd;
	gcd.param_obj = seg;
	c.call(&gcd, 0);
	
	va = 0;
	error_check(segment_map(gcd.param_obj, 0, SEGMAP_READ, &va, 0, 0));
	memcpy(a, va, sizeof(*a));
	segment_unmap(va);
    } catch (error &e) {
	cprintf("ssld_call: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("ssld_call: %s\n", e.what());
	return -1;
    }
    return a->rval;
}
