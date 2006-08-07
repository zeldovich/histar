extern "C" {
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/gatefile.h>
#include <inc/gatefilesrv.h>
#include <inc/gateparam.h>
#include <inc/labelutil.h>
#include <inc/chardevs.h>
#include <inc/pt.h>

#include <errno.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>

static int64_t
gatefile_send(struct cobj_ref gate, struct gatefd_args *args, label *ds)
{
    struct gate_call_data gcd;
    struct gatefd_args *args2 = (struct gatefd_args *) &gcd.param_buf[0];
    memcpy(args2, args, sizeof(*args2));
    try {
	label ver(3);
	ver.set(start_env->process_grant, LB_LEVEL_STAR);
	gate_call(gate, 0, ds, 0).call(&gcd, &ver);
    } catch (std::exception &e) {
	cprintf("gatefd_send:: %s\n", e.what());
	errno = EPERM;
	return -1;
    }
    memcpy(args, args2, sizeof(*args));
    return 0;
}

extern "C" int
gatefile_open(struct cobj_ref gate, int flags)
{
    struct gatefd_args args;
    args.call.op = gf_call_open;
    args.call.arg = start_env->process_grant;
    if (gatefile_send(gate, &args, 0) < 0)
	return -1;
    
    switch (args.ret.op) {
    case gf_ret_error:
	break;
    case gf_ret_null:
	return jos_devnull_open(flags);
    case gf_ret_ptm:
	return ptm_open(args.ret.obj0, args.ret.obj1, flags);
    case gf_ret_pts:
	return pts_open(args.ret.obj0, args.ret.obj1, flags);
    default:
	cprintf("gatefile_open: unknown return op %d\n", args.ret.op);
	break;
    }

    errno = EACCES;
    return -1;
}
