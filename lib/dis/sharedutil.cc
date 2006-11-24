extern "C" {
#include <inc/lib.h>
#include <inc/dis/share.h>

#include <inc/gateparam.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <string.h>
#include <errno.h>
}

#include <inc/cpplabel.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>

int64_t
gate_send(struct cobj_ref gate, void *args, int n, label *ds)
{
    struct gate_call_data gcd;
    void *args2 = &gcd.param_buf[0];
    memcpy(args2, args, n);
    try {
	gate_call(gate, 0, ds, 0).call(&gcd, 0);
    } catch (std::exception &e) {
	cprintf("share_server_gate_send: gate_call: %s\n", e.what());
	errno = EPERM;
	return -1;
    }
    memcpy(args, args2, n);
    return 0;
}

void 
shared_grant_cat(uint64_t shared_id, uint64_t cat)
{
    int64_t gt;
    error_check(gt = container_find(shared_id, kobj_gate, "user gate"));

    struct share_args args;
    args.op = share_add_local_cat;
    args.add_local_cat.cat = cat;
    label dl(1);
    dl.set(cat, LB_LEVEL_STAR);
    error_check(gate_send(COBJ(shared_id, gt), &args, sizeof(args), &dl));
}
