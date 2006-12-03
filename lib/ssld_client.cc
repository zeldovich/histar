extern "C" {
#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/gateparam.h>
#include <inc/fd.h>
#include <inc/netd.h>
#include <inc/bipipe.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>
#include <inc/spawn.hh>
#include <inc/ssldclnt.hh>

void
ssld_taint_cow(struct cobj_ref cow_gate,
	       struct cobj_ref cipher_biseg, struct cobj_ref plain_biseg,
	       uint64_t root_ct, uint64_t taint)
{
    label cs(LB_LEVEL_STAR);
    cs.set(taint, 3);
    label dr(0);
    dr.set(taint, 3);
    
    gate_call c(cow_gate, &cs, 0, &dr);
    
    struct gate_call_data gcd;
    gcd.declassify_gate = COBJ(0, 0);
    struct ssld_cow_op *op = (struct ssld_cow_op *)gcd.param_buf;
    op->cipher_biseg = cipher_biseg;
    op->plain_biseg = plain_biseg;
    op->root_ct = root_ct;
    
    c.call(&gcd, 0);
    error_check(op->rval);
}
