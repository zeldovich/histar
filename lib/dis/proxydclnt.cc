extern "C" {
#include <inc/gateparam.h>
#include <inc/lib.h>
}

#include <lib/dis/proxydclnt.hh>

#include <inc/gateclnt.hh>
#include <inc/error.hh>


void
proxyd_addmapping(char *ghandle, uint64_t handle)
{
    gate_call_data gcd;
    int64_t dir_ct, dir_gt;
    error_check(dir_ct = container_find(start_env->root_container, kobj_container, "proxyd"));
    error_check(dir_gt = container_find(dir_ct, kobj_gate, "proxyd server"));
    
    gate_call(COBJ(dir_ct, dir_gt), 0, 0, 0).call(&gcd, 0);
}
