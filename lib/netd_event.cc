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

struct cobj_ref socket_seg;

static void
netd_event_init(void)
{
    struct fs_inode netd_ct_ino;
    error_check(fs_namei("/netd", &netd_ct_ino));
    
    uint64_t netd_ct = netd_ct_ino.obj.object;

    int64_t seg_id;
    error_check(seg_id = container_find(netd_ct, kobj_segment, "sockets"));
    
    socket_seg = COBJ(netd_ct, seg_id);
}

int
netd_probe(struct Fd *fd, dev_probe_t probe)
{
    try {    
	if (socket_seg.object == 0)
	    netd_event_init();
	
	struct lwip_socket *lw = 0;
	error_check(segment_map(socket_seg, 0, SEGMAP_READ,			
				(void **)&lw, 0, 0));
	scope_guard2<int, void *, int> x(segment_unmap_delayed, lw, 1);
	
	if (probe == dev_probe_read)
	    return lw[fd->fd_sock.s].rcvevent || lw[fd->fd_sock.s].lastdata;
	else 
	    return lw[fd->fd_sock.s].sendevent;

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
    static uint64_t rcv_offset = offsetof(struct lwip_socket, rcvevent);
    static uint64_t send_offset = offsetof(struct lwip_socket, sendevent);
					  
    try {
	if (socket_seg.object == 0)
	    netd_event_init();
	
	uint64_t offset = fd->fd_sock.s * sizeof(struct lwip_socket);
	if (probe == dev_probe_read)
	    WS_SETOBJ(wstat, socket_seg, offset + rcv_offset);
	else
	    WS_SETOBJ(wstat, socket_seg, offset + send_offset);
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
