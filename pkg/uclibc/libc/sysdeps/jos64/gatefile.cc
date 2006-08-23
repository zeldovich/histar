extern "C" {
#include <inc/fd.h>
#include <inc/lib.h>
#include <inc/gatefile.h>
#include <inc/gatefilesrv.h>
#include <inc/gateparam.h>
#include <inc/labelutil.h>
#include <inc/syscall.h>
#include <inc/chardevs.h>
#include <inc/pt.h>
#include <inc/error.h>

#include <errno.h>
#include <string.h>
}

#include <inc/error.hh>
#include <inc/gateclnt.hh>


static const uint32_t gatefile_pn_count = 1;
static char gatefile_pn[gatefile_pn_count][32] = {  
    "/dev/*"
};

static int64_t
gatefile_send(struct cobj_ref gate, struct gatefd_args *args)
{
    struct gate_call_data gcd;
    struct gatefd_args *args2 = (struct gatefd_args *) &gcd.param_buf[0];
    memcpy(args2, args, sizeof(*args2));
    try {
	label ds(3);
	ds.set(start_env->process_grant, LB_LEVEL_STAR);
	gate_call(gate, 0, &ds, 0).call(&gcd, &ds);
    } catch (std::exception &e) {
	cprintf("gatefile_send:: %s\n", e.what());
	errno = EPERM;
	return -1;
    }
    memcpy(args, args2, sizeof(*args));
    return 0;
}

static char
gatefile_check(const char *pn)
{
    for (uint32_t i = 0; i < gatefile_pn_count; i++) {
	uint32_t n = strlen(gatefile_pn[i]);
	if (gatefile_pn[i][n - 1] == '*') {
	    if (strlen(pn) < n - 1)
		continue;
	    if (!memcmp(pn, gatefile_pn, n - 1))
		return 1;
	} else if (!strcmp(pn, gatefile_pn[i]))
	    return 1;
    }
    return 0;
}

extern "C" int
gatefile_open(const char *pn, int flags)
{
    // if not in gatefile_pn, return devnull so some directory listing
    // functions work correctly...
    if (!gatefile_check(pn))
	return jos_devnull_open(flags);

    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r == -E_NOT_FOUND) {
	__set_errno(ENOENT);
	return -1;
    } else if ( r < 0) {
	__set_errno(EPERM);
	return -1;
    }
	   
    struct gatefd_args args;
    args.call.op = gf_call_open;
    args.call.arg = start_env->process_grant;
    if (gatefile_send(ino.obj, &args) < 0)
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
