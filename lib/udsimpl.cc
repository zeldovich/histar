extern "C" {
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/assert.h>
#include <inc/multisync.h>
#include <inc/jcomm.h>
#include <inc/labelutil.h>
#include <inc/udsimpl.h>

#include <string.h>
#include <fcntl.h>
#include <errno.h>
}

#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>
#include <inc/scopeguard.hh>
#include <inc/privstore.hh>
#include <inc/labelutil.hh>

#define UDS_JCOMM_CT start_env->shared_container
#define UDS_JCOMM(fd) JCOMM(UDS_JCOMM_CT, fd->fd_uds.uds_jc)

struct uds_gate_args {
    struct jcomm_ref jr;
    uint64_t taint;
    uint64_t grant;
    int type;
    
    int ret;
};

static int
errno_val(uint64_t e)
{
    errno = e;
    return -1;
}

static uint32_t
max_slots(struct Fd *fd)
{
    return (sizeof(fd->fd_uds.uds_slots) / sizeof(fd->fd_uds.uds_slots[0]));
}

static void __attribute__((noreturn))
uds_stream_gate(uint64_t arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct Fd *fd = (struct Fd *) (uintptr_t) arg;
    uds_gate_args *a = (uds_gate_args *)parm->param_buf;
    struct uds_slot *slot = 0;
    
    if (!fd->fd_uds.uds_listen || fd->fd_uds.uds_type != a->type) {
	a->ret = ECONNREFUSED;
	gr->ret(0, 0, 0);
    }
    
    jthread_mutex_lock(&fd->fd_uds.uds_mu);
    for (uint32_t i = 0; i < fd->fd_uds.uds_backlog; i++) {
	if (fd->fd_uds.uds_slots[i].op == 0) {
	    slot = &fd->fd_uds.uds_slots[i];
	    break;
	}
    }

    if (slot == 0) {
	a->ret = ETIMEDOUT;
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	gr->ret(0, 0, 0);
    }

    saved_privilege sp(start_env->process_grant, 
		       a->taint, 
		       a->grant,
		       start_env->proc_container);
    sp.set_gc(false);
    
    slot->op = 1;
    memcpy(&slot->jr, &a->jr, sizeof(slot->jr));
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
	sys_sync_wait(&slot->op, 1, UINT64(~0));
	jthread_mutex_lock(&fd->fd_uds.uds_mu);
    }

    slot->op = 0;
    jthread_mutex_unlock(&fd->fd_uds.uds_mu);

    a->ret = 0;
    gr->ret(0, 0, 0);
}

int
uds_socket(int domain, int type, int protocol)
{
    if (type != SOCK_STREAM && type != SOCK_DGRAM)
	return errno_val(ENOSYS);
    if (protocol != 0)
	return errno_val(ENOSYS);
    
    struct Fd *fd;
    int r = fd_alloc(&fd, "unix-domain");
    if (r < 0)
	return errno_val(ENOMEM);
    memset(&fd->fd_uds, 0, sizeof(fd->fd_uds));
    
    fd->fd_dev_id = devuds.dev_id;
    fd->fd_omode = O_RDWR;

    fd->fd_uds.uds_backlog = max_slots(fd);
    fd->fd_uds.uds_type = type;
    fd->fd_uds.uds_prot = protocol;
    return fd2num(fd);
}

int
uds_close(struct Fd *fd)
{
    int r;

    if (fd->fd_uds.uds_gate.object) {
	r = sys_obj_unref(fd->fd_uds.uds_gate);
	if (r < 0)
	    cprintf("uds_close: unable to unref gate: %s\n", e2s(r));
    }
    
    if (fd->fd_uds.uds_connect) {
	struct jcomm_ref jr = UDS_JCOMM(fd);
	r = jcomm_shut(jr, JCOMM_SHUT_RD | JCOMM_SHUT_WR);
	if (r < 0)
	    cprintf("uds_close: jcomm_shut error: %s\n", e2s(r));
	r = jcomm_unref(jr);
	if (r < 0)
	    cprintf("uds_close: jcomm_unref error: %s\n", e2s(r));
    }
    return 0;
}

int
uds_bind(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct fs_inode ino;

    if (fd->fd_uds.uds_file.obj.object != 0)
	return errno_val(EINVAL);

    char *pn = (char *)addr->sa_data;
    r = fs_namei(pn, &ino);
    if (r == -E_NOT_FOUND) {
	char *pn2;
	const char *dn, *fn;
	struct fs_inode dir_ino;
	pn2 = strdup(pn);
	scope_guard<void, void *> fstr(free, pn2);
	fs_dirbase(pn2, &dn, &fn);
	r = fs_namei(dn, &dir_ino);
	if (r < 0)
	    return errno_val(ENOTDIR);

	label l(1);
	if (start_env->user_grant)
	    l.set(start_env->user_grant, 0);
	
	r = fs_mknod(dir_ino, fn, devuds.dev_id, 0, &ino, l.to_ulabel());
	if (r < 0)
	    return errno_val(EACCES);

	assert(!fd->fd_uds.uds_gate.object);
	struct cobj_ref gate;
	try {
	    uint64_t ct = start_env->shared_container;
	    if (fd->fd_uds.uds_type == SOCK_STREAM)
		gate = gate_create(ct, "uds", 0, 0, 0, uds_stream_gate, (uintptr_t) fd);
	    else
		throw error(-1, "XXX");
	} catch (error &e) {
	    fs_remove(dir_ino, fn, ino);
	    return errno_val(EACCES);
	} catch (std::exception &e) {
	    cprintf("uds_bind: error: %s\n", e.what());
	    fs_remove(dir_ino, fn, ino);
	    return errno_val(EACCES);
	}

	r = fs_pwrite(ino, &gate, sizeof(gate), 0);
	if (r < 0) {
	    sys_obj_unref(gate);
	    fs_remove(dir_ino, fn, ino);
	    return errno_val(EACCES);
	}
	fd->fd_uds.uds_gate = gate;
    } else if (r == 0)
	return errno_val(EEXIST);
    else
	return errno_val(EINVAL);
    
    fd->fd_uds.uds_file = ino;
    return 0;
}

int
uds_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    struct wait_stat ws[fd->fd_uds.uds_backlog];
    struct uds_slot *slots = fd->fd_uds.uds_slots;
    struct uds_slot *os;

    if (fd->fd_uds.uds_type != SOCK_STREAM)
	return errno_val(EOPNOTSUPP);
    
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
		os = &slots[i];
		goto out;
	    }
	
	jthread_mutex_unlock(&fd->fd_uds.uds_mu);
	multisync_wait(ws, fd->fd_uds.uds_backlog, UINT64(~0));
    }

 out:
    scope_guard<void, jthread_mutex_t *> 
	unlock(jthread_mutex_unlock, &fd->fd_uds.uds_mu);

    saved_privilege sp(os->taint, os->grant, os->priv_gt);
    sp.set_gc(true);
    sp.acquire();
            
    struct Fd *nfd;
    int r = fd_alloc(&nfd, "unix-domain");
    if (r < 0)
	return errno_val(ENOMEM);
    memset(&nfd->fd_uds, 0, sizeof(nfd->fd_uds));
    nfd->fd_dev_id = devuds.dev_id;
    nfd->fd_omode = O_RDWR;
    nfd->fd_uds.uds_type = fd->fd_uds.uds_type;
    nfd->fd_uds.uds_prot = fd->fd_uds.uds_prot;
    
    r = jcomm_addref(os->jr, UDS_JCOMM_CT);
    if (r < 0) {
	cprintf("uds_accept: unable to addref jcomm: %s\n", e2s(r));
	jos_fd_close(nfd);
	return errno_val(EINVAL);
    }
    nfd->fd_uds.uds_connect = 1;
    memcpy(&nfd->fd_uds.uds_jc, &os->jr.jc, sizeof(nfd->fd_uds.uds_jc));
    fd_set_extra_handles(fd, os->grant, os->taint);
    
    os->op = 2;
    sys_sync_wakeup(&os->op);

    return fd2num(nfd);
}

static int
uds_read_gate(struct Fd *fd, const struct sockaddr *addr, cobj_ref *gate)
{
    int r;
    struct fs_inode ino;
    char *pn = (char *)addr->sa_data;
    
    r = fs_namei(pn, &ino);
    if (r < 0)
	return errno_val(ENOENT);
    
    struct fs_object_meta m;
    r = sys_obj_get_meta(ino.obj, &m);
    if (r < 0)
	return errno_val(EACCES);

    if (m.dev_id != devuds.dev_id)
	return errno_val(ECONNREFUSED);

    r = fs_pread(ino, gate, sizeof(*gate), 0);
    if (r < 0)
	return errno_val(EACCES);
    else if (r != sizeof(*gate))
	/* a uds dev was just created, and nobody is listening yet */
	return errno_val(ECONNREFUSED);
    
    return 0;
}

int
uds_connect(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct cobj_ref gate;
    r = uds_read_gate(fd, addr, &gate);
    if (r < 0)
	return r;

    struct gate_call_data gcd;
    memset(&gcd, 0, sizeof(gcd));
    uds_gate_args *a = (uds_gate_args *)gcd.param_buf;
    label l(1);
    
    uint64_t taint = handle_alloc();
    uint64_t grant = handle_alloc();
    scope_guard2<void, uint64_t, uint64_t> 
	drop(thread_drop_starpair, taint, grant);
    
    l.set(taint, 3);
    l.set(grant, 0);

    a->taint = taint;
    a->grant = grant;
    a->type = fd->fd_uds.uds_type;
    struct jcomm_ref jr;
    uint16_t mode = 0;
    
    if (fd->fd_uds.uds_type == SOCK_DGRAM)
	mode |= JCOMM_PACKET;
    
    r = jcomm_alloc(UDS_JCOMM_CT, l.to_ulabel(), mode, 
		    &jr, &a->jr);
    fd->fd_uds.uds_jc = jr.jc;
    
    if (r < 0)
	return r;

    l.set(taint, LB_LEVEL_STAR);
    l.set(grant, LB_LEVEL_STAR);
    try {
	gate_call(gate, 0, &l, 0).call(&gcd, 0);
    } catch (error &e) {
	return errno_val(EINVAL);
    } catch (std::exception &e) {
	cprintf("uds_gate_connect: error: %s\n", e.what());
	return errno_val(EINVAL);
    }

    if (a->ret != 0) {
	jcomm_unref(jr);
	memset(&fd->fd_uds.uds_jc, 0, sizeof(fd->fd_uds.uds_jc));
	return errno_val(a->ret);
    }

    fd->fd_uds.uds_connect = 1;
    fd_set_extra_handles(fd, grant, taint);
    
    drop.dismiss();
    return 0;
}

int
uds_listen(struct Fd *fd, int backlog)
{
    if (fd->fd_uds.uds_type != SOCK_STREAM)
	return errno_val(EOPNOTSUPP);

    if ((uint32_t)backlog > max_slots(fd))
	return errno_val(EINVAL);

    fd->fd_uds.uds_backlog = backlog;
    fd->fd_uds.uds_listen = 1;

    return 0;
}

ssize_t
uds_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    int r;
    if (fd->fd_uds.uds_type != SOCK_STREAM)
	return errno_val(ENOSYS);

    if (!fd->fd_uds.uds_connect)
	return errno_val(EINVAL);
    
    r = jcomm_write(UDS_JCOMM(fd), buf, count);
    if (r == -E_AGAIN)
	return errno_val(EAGAIN);
    else if (r == -E_EOF)
	/* XXX deliver SIGPIPE */
	return errno_val(EPIPE);
    else if (r < 0)
	return errno_val(EIO);

    return r;
}

ssize_t
uds_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    int r;
    if (fd->fd_uds.uds_type != SOCK_STREAM)
	return errno_val(ENOSYS);

    if (!fd->fd_uds.uds_connect)
	return errno_val(EINVAL);

    r = jcomm_read(UDS_JCOMM(fd), buf, count);
    if (r == -E_AGAIN)
	return errno_val(EAGAIN);
    else if (r == -E_EOF)
	return 0;
    else if (r < 0)
	return errno_val(EIO);

    return r;
}

int
uds_onfork(struct Fd *fd, uint64_t ct)
{
    int r = jcomm_addref(UDS_JCOMM(fd), ct);
    if (r < 0)
	cprintf("uds_onfork: jcomm_addref error: %s\n", e2s(r));
    return r;
}
