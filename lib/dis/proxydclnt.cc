extern "C" {
#include <inc/gateparam.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <string.h>
#include <stdio.h>
}

#include <lib/dis/proxydclnt.hh>
#include <lib/dis/proxydsrv.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/error.hh>


static cobj_ref
proxyd_server(void)
{
    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "proxyd"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "proxyd server"));
    
    return COBJ(dir_ct, dir_gt);
}

int
proxyd_add_mapping(char *global, uint64_t local, 
                  uint64_t grant, uint8_t grant_level)
{
    gate_call_data gcd;
    proxyd_args *args = (proxyd_args *) gcd.param_buf;

    cobj_ref server_gate = proxyd_server();
    
    uint64_t size = strlen(global) + 1;
    if (size > sizeof(args->mapping.global))
        throw error(-E_NO_SPACE,"%s too big (%ld > %ld)", global, 
                    size, sizeof(args->mapping.global)); 
        
    strcpy(args->mapping.global, global);
    args->mapping.local = local;
    args->mapping.grant = grant;
    args->mapping.grant_level = grant_level;
    args->op = proxyd_mapping;

    // XXX
    label th_cl;
    thread_cur_label(&th_cl);
    gate_call(server_gate, 0, &th_cl, 0).call(&gcd, 0);
    return args->ret;
}

int 
proxyd_get_local(char *global, uint64_t *local) 
{
    gate_call_data gcd;
    proxyd_args *args = (proxyd_args *) gcd.param_buf;

    cobj_ref server_gate = proxyd_server();

    uint64_t size = strlen(global) + 1;    
    if (size > sizeof(args->handle.global))
        throw error(-E_NO_SPACE,"%s too big (%ld > %ld)", global, 
                    size, sizeof(args->handle.global)); 
    
    strcpy(args->handle.global, global);
    args->op = proxyd_local;

    // XXX        
    label th_cl;
    thread_cur_label(&th_cl);
    gate_call(server_gate, 0, &th_cl, 0).call(&gcd, 0);
    if (args->ret < 0) {
        *local = 0;
        return args->ret;    
    }
    *local = args->handle.local;
    return 0;
}

int 
proxyd_get_global(uint64_t local, char *ret) 
{
    gate_call_data gcd;
    proxyd_args *args = (proxyd_args *) gcd.param_buf;

    cobj_ref server_gate = proxyd_server();

    args->handle.local = local;
    memset(args->handle.global, 0, sizeof(args->handle.global));
    args->op = proxyd_global;

    // XXX        
    label th_cl;
    thread_cur_label(&th_cl);
    gate_call(server_gate, 0, &th_cl, 0).call(&gcd, 0);
    
    if (args->ret < 0)
        return args->ret;
    
    if (strlen(args->handle.global))
        strcpy(ret, args->handle.global);
    else
        return -E_INVAL;
        
    return 0;
}
