extern "C" {
#include <stdio.h>    
}

#include <lib/dis/proxydsrv.hh>

#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>


static void __attribute__((noreturn))
proxyd_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gr->ret(0, 0, 0);    
}

struct 
cobj_ref proxydsrv_create(uint64_t gate_container, const char *name,
                label *label, label *clearance)
{
    return gate_create(gate_container,
        "proxyd server", label, clearance,
        &proxyd_srv, 0);
}
