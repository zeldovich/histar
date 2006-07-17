extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <inc/dis/omd.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>

static void __attribute__((noreturn))
om_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    gate_call_data_copy(&bck, parm);
    om_args *args = (om_args*) parm->param_buf;
    
    switch(args->op) {
    case om_observe:
	args->ret = 0;
	break;
    case om_modify:
	args->ret = 0;
	break;
    }
    
    gate_call_data_copy(parm, &bck);
    gr->ret(0, 0, 0);    
}

// need a table mapping local cats -> grant gates
// need a table mapping foriegn cats -> grant gates

static void __attribute__((noreturn))
admin_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gr->ret(0, 0, 0);
}

int
main (int ac, char **av)
{
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);

    gate_create(start_env->shared_container,"om gate", &th_l, 
                &th_cl, &om_gate, 0);
    
    gate_create(start_env->shared_container,"admin gate", &th_l, 
		&th_cl, &admin_gate, 0);
    
    printf("omd: init\n");

    thread_halt();
    return 0;    
}
