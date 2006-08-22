extern "C" {
#include <inc/lib.h>
#include <inc/fs.h>
#include <inc/stdio.h>
#include <inc/gateparam.h>
#include <inc/error.h>

#include <string.h> 
}

#include <inc/error.hh>
#include <inc/rbac.hh>
#include <inc/gateclnt.hh>
#include <inc/labelutil.hh>
#include <inc/selftaint.hh>

namespace rbac {

struct cobj_ref 
fs_object(const char *pn)
{
    struct fs_inode ino;
    error_check(fs_namei(pn, &ino));
    return ino.obj;
}

struct cobj_ref
gate_send(struct cobj_ref gate, struct cobj_ref arg)
{
    struct gate_call_data gcd;
    gcd.param_obj = arg;
    try {
	label ds(3);
	ds.set(start_env->process_grant, LB_LEVEL_STAR);
	gate_call(gate, 0, &ds, 0).call(&gcd, &ds);
    } catch (std::exception &e) {
	cprintf("rbac::gate_send: error: %s\n", e.what());
	throw e;
    }
    return gcd.param_obj;
}

void
gate_send(struct cobj_ref gate, void *args, uint64_t n)
{
    struct gate_call_data gcd;
    if (n > sizeof(gcd.param_buf))
	throw error(-E_NO_SPACE, "%ld > %ld", n, sizeof(gcd.param_buf));
    
    void *args2 = (void *) &gcd.param_buf[0];
    memcpy(args2, args, n);
    try {
	label ds(3);
	ds.set(start_env->process_grant, LB_LEVEL_STAR);
	gate_call(gate, 0, &ds, 0).call(&gcd, &ds);
    } catch (std::exception &e) {
	cprintf("rbac::gate_send: error %s\n", e.what());
	throw e;
    }
    memcpy(args, args2, n);
}

} // namespace rbac

////
// role_gate
////

role_gate::role_gate(const char *pn)
{
    uint64_t role_ct = rbac::fs_object(pn).object;
    uint64_t acquire_gt, trans_gt;
    
    error_check(acquire_gt = container_find(role_ct, kobj_gate, "acquire"));
    error_check(trans_gt = container_find(role_ct, kobj_gate, "trans"));

    acquire_gate_ = COBJ(role_ct, acquire_gt);
    trans_gate_ = COBJ(role_ct, trans_gt);
}

void
role_gate::execute(struct trans *trans)
{
    rbac::gate_send(trans_gate_, trans->trans_gate);
}

void
role_gate::acquire(void)
{
    int64_t taint_role = 0;
    try {
	rbac::gate_send(acquire_gate_, &taint_role, sizeof(taint_role));
	if (taint_role < 0)
	    throw error(taint_role, "unable to acquire role");
	else if (taint_role == 0)
	    throw error(-E_UNSPEC, "unable to acquire role");
	
	label t(1);
	t.set(taint_role, 3);
	taint_self(&t);
    } catch (std::exception &e) {
	cprintf("role_gate::acquire: error: %s\n", e.what());
	throw e;
    }
}

void
self_assign_role(role_gate *role)
{
    role->acquire();
}

struct trans
trans_lookup(const char *pn)
{
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r < 0)
	throw error(r, "cannot fs_namei %s", pn);
    struct trans ret = { ino.obj };
    return ret;
}
