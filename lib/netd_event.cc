extern "C" {
#include <inc/lib.h>
#include <inc/multisync.h>
#include <inc/netdevent.h>
#include <inc/error.h>
#include <inc/fd.h>
#include <inc/netd.h>
#include <api/socketdef.h>

#include <stddef.h>
}

#include <inc/error.hh>
#include <inc/scopeguard.hh>

static struct lwip_socket *lw;

static void
netd_event_init(void)
{
    struct fs_inode netd_ct_ino;
    error_check(fs_namei("/netd", &netd_ct_ino));
    
    uint64_t netd_ct = netd_ct_ino.obj.object;

    int64_t seg_id;
    error_check(seg_id = container_find(netd_ct, kobj_segment, "sockets"));
    
    cobj_ref socket_seg = COBJ(netd_ct, seg_id);
    error_check(segment_map(socket_seg, 0, SEGMAP_READ,			
			    (void **)&lw, 0, 0));
}

int
netd_slow_probe(struct Fd *fd, dev_probe_t probe)
{
    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, probe) + sizeof(a.probe);
    a.op_type = netd_op_probe;
    a.probe.fd = fd->fd_sock.s;
    a.probe.write = probe == dev_probe_write ? 1 : 0;
    return netd_call(fd->fd_sock.netd_gate, &a);
}

int
netd_probe(struct Fd *fd, dev_probe_t probe)
{
    static char sanity_check = 0;

    try {    
	if (!lw)
	    netd_event_init();
	
	int slow_probe = 0;
	if (sanity_check) 
	    slow_probe = netd_slow_probe(fd, probe);

	if (probe == dev_probe_read) {
	    int r = lw[fd->fd_sock.s].rcvevent || lw[fd->fd_sock.s].lastdata;
	    if (sanity_check && !r) 
		assert(!slow_probe);
	    return r;
	} else {
	    int r = lw[fd->fd_sock.s].sendevent;
	    if (sanity_check && !r)
		assert(!slow_probe);
	    return r;
	}
    } catch (error &e) {
	cprintf("netd_probe: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("netd_probe: %s\n", e.what());
	return -E_UNSPEC;
    } catch (...) {
	return -E_UNSPEC;
    }
    return 0;
}

static int
msync_cb(void *arg0, dev_probe_t probe, volatile uint64_t *addr, void **arg1)
{
    struct Fd *fd = (struct Fd*)arg0;

    if (!lw)
	netd_event_init();
    if (probe == dev_probe_write && lw[fd->fd_sock.s].send_wakeup)
	return 0;
    if (probe == dev_probe_read && lw[fd->fd_sock.s].recv_wakeup)
	return 0;

    struct netd_op_args a;
    a.size = offsetof(struct netd_op_args, notify) + sizeof(a.notify);
    a.op_type = netd_op_notify;
    a.notify.fd = fd->fd_sock.s;
    a.notify.write = probe == dev_probe_write ? 1 : 0;
    return netd_call(fd->fd_sock.netd_gate, &a);
}

int
netd_wstat(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat)
{
    try {
	if (!lw)
	    netd_event_init();

	if (probe == dev_probe_read)
	    WS_SETADDR(wstat, &lw[fd->fd_sock.s].rcvevent);
	else
	    WS_SETADDR(wstat, &lw[fd->fd_sock.s].sendevent);
	WS_SETVAL(wstat, 0);
	WS_SETCBARG(wstat, fd);
	WS_SETCB0(wstat, &msync_cb);
    } catch (error &e) {
	cprintf("netd_wstat: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("netd_wstat: %s\n", e.what());
	return -E_UNSPEC;
    } catch (...) {
	return -E_UNSPEC;
    }
    return 0;
}
