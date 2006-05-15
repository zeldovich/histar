extern "C" {
#include <inc/types.h>    
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
}

#include <inc/dis/cad.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/cpplabel.hh>
#include <inc/gatesrv.hh>
#include <inc/gateclnt.hh>

////
// gate servers
///

static uint64_t sign_ct;
static uint64_t ver_ct;

static void __attribute__((noreturn))
sign_manager(void *a, struct gate_call_data *parm, gatesrv_return *gr)
{
    auth_agent_arg *arg = (auth_agent_arg *) parm->param_buf;
    switch (arg->op) {
        case aa_new: {
            uint64_t ct;
            label ct_l(1);
            ct_l.set(arg->agent_grant, 0);

            error_check(ct = sys_container_alloc(sign_ct,
                         ct_l.to_ulabel(), arg->name,
                        0, CT_QUOTA_INF));
            arg->agent_ct = ct;
            break;    
        }
    }
    printf("hello from sign!\n");
    gr->ret(0, 0, 0);    
}

static void __attribute__((noreturn))
auth_agent_ver(void *a, struct gate_call_data *parm, gatesrv_return *gr)
{
    auth_agent_arg *arg = (auth_agent_arg *) parm->param_buf;
    switch (arg->op) {
        case aa_new: {
            uint64_t ct;
            // XXX should set certificate-authority-grant to 0
            label ct_l(1);

            error_check(ct = sys_container_alloc(ver_ct,
                         ct_l.to_ulabel(), arg->name,
                        0, CT_QUOTA_INF));
            arg->agent_ct = ct;
            break;    
        }
    }
    printf("hello from ver!\n");
    gr->ret(0, 0, 0);    
}

int
main(int ac, char **av) 
{
    uint64_t grant0 = handle_alloc();
    uint64_t grant1 = handle_alloc();

    label ct0(1);
    ct0.set(grant0, 0);
    error_check(sign_ct = sys_container_alloc(start_env->shared_container,
                 ct0.to_ulabel(), "sign",
                 0, CT_QUOTA_INF));


    label l0, cl0;
    thread_cur_label(&l0);
    l0.set(grant1, 1);
    thread_cur_clearance(&cl0);
    gate_create(sign_ct, "sign gate", &l0, 
                                &cl0, &sign_manager, 0);


    label ct1(1);
    // XXX should set certificate-authority-grant to 0
    ct1.set(grant1, 0);
    error_check(ver_ct = sys_container_alloc(start_env->shared_container,
                 ct1.to_ulabel(), "ver",
                 0, CT_QUOTA_INF));


    label l1, cl1;
    thread_cur_label(&l1);
    l1.set(grant0, 1);
    // XXX should set certificate-authority-grant to 0
    thread_cur_clearance(&cl1);
    gate_create(ver_ct, "ver gate", &l1, 
                       &cl1, &auth_agent_ver, 0);

    thread_drop_star(grant0);
    thread_drop_star(grant1);
    printf("cad: ready\n");
    thread_halt();
}
