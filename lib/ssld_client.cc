extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/ssld.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/fd.h>
#include <inc/netd.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/spawn.hh>
#include <inc/ssldclnt.hh>

struct cobj_ref
ssld_cow_call(struct cobj_ref gate, uint64_t root_ct, 
	      label *cs, label *ds, label *dr)
{
    gate_call c(gate, cs, ds, dr);
    
    struct gate_call_data gcd;
    int64_t *arg = (int64_t *)gcd.param_buf;
    *arg = root_ct;
    
    c.call(&gcd, 0);
    error_check(*arg);
    
    return COBJ(root_ct, *arg);
}

extern "C" int
ssld_call(struct cobj_ref gate, struct ssld_op_args *a)
{
    try {
	gate_call c(gate, 0, 0, 0);
	
	struct cobj_ref seg;
	void *va = 0;
	error_check(segment_alloc(c.call_ct(), sizeof(*a), &seg, &va,
				  0, "ssld_call() args"));
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
