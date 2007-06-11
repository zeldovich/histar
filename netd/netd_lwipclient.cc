extern "C" {
#include <inc/error.h>
#include <inc/netd.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/stdio.h>
#include <netd/netdlwip.h>

#include <string.h>
#include <errno.h>
}

#include <inc/error.hh>
#include <netd/netdclnt.hh>

static int
gate_lookup(const char *bn, const char *gn, struct cobj_ref *ret)
{
    struct fs_inode netd_ct_ino;
    int r = fs_namei(bn, &netd_ct_ino);
    if (r == 0) {
	uint64_t netd_ct = netd_ct_ino.obj.object;
	
	int64_t gate_id = container_find(netd_ct, kobj_gate, gn);
	if (gate_id > 0) {
	    *ret = COBJ(netd_ct, gate_id);
	    return 0;
	}
    }
    return -E_NOT_FOUND;
}

int
netd_lwip_client_init(struct cobj_ref *gate, struct cobj_ref *fast_gate)
{
    int r = gate_lookup("/netd", "netd", gate);
    if (r < 0)
	return r;
    return gate_lookup("/netd", "netd-fast", fast_gate);
}

int
netd_lwip_call(struct Fd *fd, struct netd_op_args *a)
{
    static int do_fast_calls;

    if (a->op_type == netd_op_ioctl)
	return netd_lwip_ioctl(&a->ioctl);
    else if (a->op_type == netd_op_probe)
	return netd_lwip_probe(fd, &a->probe);
    else if (a->op_type == netd_op_statsync)
	return netd_lwip_statsync(fd, &a->statsync);
    
    // A bit of a hack because we need to get tainted first...
    if (do_fast_calls) {
	try {
	    netd_fast_call(a);

	    if (a->rval < 0)
		errno = a->rerrno;
	    return a->rval;
	} catch (std::exception &e) {
	    cprintf("netd_lwip_call: cannot fast-call: %s\n", e.what());
	}
    }

    try {
	int r;
	r = netd_slow_call(fd->fd_sock.netd_gate, a);

	if (a->rval >= 0)
	    do_fast_calls = 1;
	
	return r;
    } catch (error &e) {
	cprintf("netd_lwip_call: %s\n", e.what());
	return e.err();
    }
}
