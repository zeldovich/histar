extern "C" {
#include <inc/lib.h>
#include <inc/types.h>
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
