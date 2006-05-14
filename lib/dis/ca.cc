extern "C" {
#include <inc/types.h>    
#include <inc/gateparam.h>   
}

#include <inc/dis/ca.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>



static void __attribute__((noreturn))
ca_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gr->ret(0, 0, 0);    
}

char
ca_auth(ca_response *res)
{
    return 1;    
}

cobj_ref 
ca_new(const char *name) 
{
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    
    return gate_create(start_env->shared_container, name, &th_l, 
                       &th_cl, &ca_srv, 0);
}
