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
#include <inc/labelutil.hh>

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
    struct uds_slot *slot = 0;
    
    assert(fd->fd_uds.uds_listen);
    
    jthread_mutex_lock(&fd->fd_uds.uds_mu);
    for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++) {
	if (fd->fd_uds.uds_slots[i].op == 0) {
	    slot = &fd->fd_uds.uds_slots[i];
	    break;
	}
    }

    if (slot == 0) {
	a->ret = -1;
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	gr->ret(0, 0, 0);
    }

    saved_privilege sp(start_env->process_grant, 
		       a->taint, 
		       a->grant,
		       start_env->proc_container);
    sp.set_gc(false);
    
    slot->op = 1;
    slot->bipipe_seg = a->bipipe_seg;
    slot->priv_gt = sp.gate();
    slot->taint = a->taint;
    slot->grant = a->grant;
    
    for (;;) {
	if (slot->op == 2)
	    break;
	else if (slot->op == 0)
	    panic("uds_gate: unexpected op");
	
	sys_sync_wakeup(&slot->op);
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	sys_sync_wait(&slot->op, 1, ~0UL);
	jthread_mutex_lock(&fd->fd_uds.uds_mu);
    }

    slot->op = 0;
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
    struct uds_slot *slots = fd->fd_uds.uds_slots;
    cobj_ref bs;
    uint64_t grant, taint;
    
    assert(fd->fd_uds.uds_listen);
    
    memset(ws, 0, sizeof(ws));
    for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++) {
	WS_SETADDR(&ws[i], &slots[i].op);
	WS_SETVAL(&ws[i], 0);
    }
    
    for (;;) {
	jthread_mutex_lock(&fd->fd_uds.uds_mu);
	for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++)
	    if (slots[i].op == 1) {
		saved_privilege sp(slots[i].taint, slots[i].grant, slots[i].priv_gt);
		sp.set_gc(true);
		sp.acquire();
		
		taint = slots[i].taint;
		grant = slots[i].grant; 
		
		bs = slots[i].bipipe_seg;
		slots[i].op = 2;
		sys_sync_wakeup(&slots[i].op);
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
    scope_guard2<void, uint64_t, uint64_t> 
	drop(thread_drop_starpair, taint, grant);
    

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
    fd_set_extra_handles(fd, grant, taint);
    
    unref.dismiss();
    drop.dismiss();
    return 0;
}
