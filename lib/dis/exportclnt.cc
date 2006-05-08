extern "C" {
#include <inc/gateparam.h>
#include <inc/types.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>

#include <lib/dis/exportsrv.hh>

static cobj_ref
export_server(void)
{
    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "exportd"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "export server"));
    
    return COBJ(dir_ct, dir_gt);
}

int
export_grant(uint64_t grant, uint64_t handle) 
{
    gate_call_data gcd;
    export_args *args = (export_args *) gcd.param_buf;

    cobj_ref server_gate = export_server();
    args->op = exp_grant;
    args->grant = grant;
    args->handle = handle;
    
    label dl(1);
    dl.set(args->grant, LB_LEVEL_STAR);
    gate_call(server_gate, 0, &dl, 0).call(&gcd, 0);
    
    return args->ret;    
}
