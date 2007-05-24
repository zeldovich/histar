extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/multisync.h>
#include <inc/bipipe.h>
#include <inc/labelutil.h>
#include <bits/udsgate.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/scopeguard.hh>
#include <inc/privstore.hh>

struct uds_gate_args {
    struct cobj_ref bipipe_seg;
    uint64_t taint;
    uint64_t grant;
    
    int ret;
};

static void __attribute__((noreturn))
uds_gate(uint64_t arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct Fd *fd = (struct Fd *) arg;
    uds_gate_args *a = (uds_gate_args *)parm->param_buf;
    int32_t ndx = -1;
    
    assert(fd->fd_uds.uds_listen);
    
    jthread_mutex_lock(&fd->fd_uds.uds_mu);
    for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++) {
	if (fd->fd_uds.slot[i].op == 0) {
	    ndx = i;
	    break;
	}
    }

    if (ndx < 0) {
	a->ret = -1;
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	gr->ret(0, 0, 0);
    }

    saved_privilege sp0(start_env->process_grant, 
			a->taint, 
			start_env->proc_container);
    saved_privilege sp1(start_env->process_grant, 
			a->grant, 
			start_env->proc_container);
    sp0.set_gc(false);
    sp1.set_gc(false);
    
    fd->fd_uds.slot[ndx].op = 1;
    fd->fd_uds.slot[ndx].bipipe_seg = a->bipipe_seg;
    fd->fd_uds.slot[ndx].priv_gt0 = sp0.gate();
    fd->fd_uds.slot[ndx].h0 = sp0.handle();
    fd->fd_uds.slot[ndx].priv_gt1 = sp1.gate();
    fd->fd_uds.slot[ndx].h1 = sp1.handle();
    
    for (;;) {
	if (fd->fd_uds.slot[ndx].op == 2)
	    break;
	else if (fd->fd_uds.slot[ndx].op == 0)
	    panic("uds_gate: unexpected op");
	
	sys_sync_wakeup(&fd->fd_uds.slot[ndx].op);
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	sys_sync_wait(&fd->fd_uds.slot[ndx].op, 1, ~0UL);
	jthread_mutex_lock(&fd->fd_uds.uds_mu);
    }

    fd->fd_uds.slot[ndx].op = 0;
    jthread_mutex_unlock(&fd->fd_uds.uds_mu);

    a->ret = 0;
    gr->ret(0, 0, 0);
}

int
uds_gate_new(struct Fd *fd, uint64_t ct, struct cobj_ref *gate)
{
    try {
	*gate = gate_create(ct, "uds", 0, 0, 0, uds_gate, (uint64_t)fd);
    } catch (error &e) {
	return e.err();
    } catch (std::exception &e) {
	cprintf("uds_gate_new: error: %s\n", e.what());
	return -E_INVAL;
    }
    return 0;
}

int 
uds_gate_accept(struct Fd *fd)
{
    struct wait_stat ws[fd->fd_uds.uds_backlog];
    cobj_ref bs;
    uint64_t grant, taint;
    
    assert(fd->fd_uds.uds_listen);
    
    memset(ws, 0, sizeof(ws));
    for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++) {
	WS_SETADDR(&ws[i], &fd->fd_uds.slot[i].op);
	WS_SETVAL(&ws[i], 0);
    }
    
    for (;;) {
	jthread_mutex_lock(&fd->fd_uds.uds_mu);
	for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++)
	    if (fd->fd_uds.slot[i].op == 1) {
		saved_privilege sp0(fd->fd_uds.slot[i].h0, fd->fd_uds.slot[i].priv_gt0);
		saved_privilege sp1(fd->fd_uds.slot[i].h1, fd->fd_uds.slot[i].priv_gt1);
		sp0.set_gc(true);
		sp1.set_gc(true);
		
		sp0.acquire();
		sp1.acquire();
		
		taint = fd->fd_uds.slot[i].h0;
		grant = fd->fd_uds.slot[i].h1; 
		
		bs = fd->fd_uds.slot[i].bipipe_seg;
		fd->fd_uds.slot[i].op = 2;
		sys_sync_wakeup(&fd->fd_uds.slot[i].op);
		jthread_mutex_unlock(&fd->fd_uds.uds_mu);
		goto out;
	    }
	
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	multisync_wait(ws, fd->fd_uds.uds_backlog, ~0UL);
    }

 out:
    struct Fd *nfd;
    int r = fd_alloc(&nfd, "unix-domain");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }
    memset(&nfd->fd_uds, 0, sizeof(nfd->fd_uds));
    nfd->fd_dev_id = devuds.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_uds.uds_type = fd->fd_uds.uds_type;
    nfd->fd_uds.uds_prot = fd->fd_uds.uds_prot;
    
    nfd->fd_uds.uds_connect = 1;
    nfd->fd_uds.bipipe_seg = bs;
    nfd->fd_uds.bipipe_a = 0;

    fd_set_extra_handles(fd, grant, taint);
    
    return fd2num(nfd);
}

int 
uds_gate_connect(struct Fd *fd, cobj_ref gate)
{
    int r;
    struct gate_call_data gcd;
    cobj_ref bs;
    memset(&gcd, 0, sizeof(gcd));
    uds_gate_args *a = (uds_gate_args *)gcd.param_buf;
    label l(1);
    
    uint64_t taint = handle_alloc();
    uint64_t grant = handle_alloc();

    l.set(taint, 3);
    l.set(grant, 0);

    r = bipipe_alloc(start_env->shared_container, &bs,
		     l.to_ulabel(), "uds-bipipe");
    if (r < 0)
	return r;
    scope_guard<int, cobj_ref> unref(sys_obj_unref, bs); 

    a->bipipe_seg = bs;
    a->taint = taint;
    a->grant = grant;

    l.set(taint, LB_LEVEL_STAR);
    l.set(grant, LB_LEVEL_STAR);
    try {
	gate_call(gate, 0, &l, 0).call(&gcd, 0);
    } catch (error &e) {
	return e.err();
    } catch (std::exception &e) {
	cprintf("uds_gate_new: error: %s\n", e.what());
	return -E_INVAL;
    }

    fd->fd_uds.bipipe_a = 1;
    fd->fd_uds.bipipe_seg = bs;

    unref.dismiss();
    return 0;
}
