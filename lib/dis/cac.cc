extern "C" {
#include <inc/types.h>    
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
}

#include <inc/dis/caclient.hh>
#include <inc/dis/cad.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>

typedef enum {
    va_ver,
    va_taint,
} ver_agent_op_t;

struct ver_agent_arg 
{
    ver_agent_op_t op;
    cobj_ref seg;
    // return
    int status;
};

/////////
// gate clients
/////////

static void __attribute__((noreturn))
auth_agent_sign(void *a, struct gate_call_data *parm, gatesrv_return *gr)
{
    // XXX
    gr->ret(0, 0, 0);    
}

static void __attribute__((noreturn))
auth_agent_ver(void *a, struct gate_call_data *parm, gatesrv_return *gr)
{
    ver_agent_arg *arg = (ver_agent_arg *) parm->param_buf;
    uint64_t taint = (uint64_t)a;
    
    switch (arg->op) {
        case va_ver: {
            // XXX
            label *dl = new label(3);
            dl->set(taint, LB_LEVEL_STAR);
            gr->ret(0, dl, 0);    
        }
        case va_taint: {
            label l;
            obj_get_label(arg->seg, &l);
            l.set(taint, 3);
            int64_t tseg;
            error_check(tseg = sys_segment_copy(arg->seg, arg->seg.container,
                             l.to_ulabel(), "tainted copy"));
            arg->seg = COBJ(arg->seg.container, tseg);
            break;
        }
    }
    gr->ret(0, 0, 0);    
}

void
ver_agent::verify(const void *subject, int n, void *sign, int m)
{
    gate_call_data gcd;
    ver_agent_arg *arg = (ver_agent_arg *) gcd.param_buf;
    arg->op = va_ver;
    
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);
}

cobj_ref
ver_agent::taint(cobj_ref seg)
{
    gate_call_data gcd;
    ver_agent_arg *arg = (ver_agent_arg *) gcd.param_buf;
    arg->op = va_taint;
    arg->seg = seg;
    
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("ver_agent::taint: unable to taint");
    return arg->seg;
}

sign_agent
auth_agent::sign_agent_new(const char *name, uint64_t grant)
{
    gate_call_data gcd;
    auth_agent_arg *arg = (auth_agent_arg *) gcd.param_buf;

    arg->op = aa_new;
    arg->agent_grant = grant;
    strncpy(arg->name, name, sizeof(arg->name));
    label dl(3);
    dl.set(grant, LB_LEVEL_STAR);
    gate_call(sign_, 0, &dl, 0).call(&gcd, 0);
    
    label l0, cl0;
    thread_cur_label(&l0);
    thread_cur_clearance(&cl0);
    cl0.set(grant, 0);
    
    uint64_t ct = arg->agent_ct;
    cobj_ref sign = gate_create(ct, "agent gate", &l0, 
                                &cl0, &auth_agent_sign, 0);
    return sign_agent(sign);
}

ver_agent
auth_agent::ver_agent_new(const char *name, uint64_t taint)
{
    gate_call_data gcd;
    auth_agent_arg *arg = (auth_agent_arg *) gcd.param_buf;

    arg->op = aa_new;
    strncpy(arg->name, name, sizeof(arg->name));
    gate_call(ver_, 0, 0, 0).call(&gcd, 0);
    
    // XXX
    label l0, cl0;
    thread_cur_label(&l0);
    thread_cur_clearance(&cl0);
    
    uint64_t ct = arg->agent_ct;
    cobj_ref ver = gate_create(ct, "agent gate", &l0, 
                                &cl0, &auth_agent_ver, (void*)taint);
    return ver_agent(ver);
}

auth_agent::auth_agent(const char *name) 
{
    int64_t cad_ct, sign_ct, ver_ct, sign_gt, ver_gt;
    error_check(cad_ct = container_find(start_env->root_container, kobj_container, name));

    error_check(sign_ct = container_find(cad_ct, kobj_container, "sign"));
    error_check(sign_gt = container_find(sign_ct, kobj_gate, "sign gate"));    

    error_check(ver_ct = container_find(cad_ct, kobj_container, "ver"));
    error_check(ver_gt = container_find(ver_ct, kobj_gate, "ver gate"));    
    
    sign_ = COBJ(sign_ct, sign_gt);
    ver_ = COBJ(ver_ct, ver_gt);
}
