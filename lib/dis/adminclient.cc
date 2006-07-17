extern "C" {
#include <inc/lib.h>
#include <inc/types.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/dis/adminclient.hh>

#include <inc/error.hh>
#include <inc/gateclnt.hh>

admin_client::admin_client(const char *name)
{
    name_ = strdup(name);
}

admin_client::~admin_client(void)
{
    delete name_;
}

cobj_ref
admin_client::admin_gate(void) const
{
    int64_t ct, gt;
    error_check(ct = container_find(start_env->root_container, 
				    kobj_container, name_));
    error_check(gt = container_find(ct, kobj_gate, "admin gate"));    
    return COBJ(ct, gt);
}

void
admin_client::register_cat(uint64_t cat)
{
    cobj_ref gate = admin_gate();
    gate_call_data gcd;
    //admin_args *args = (admin_args*)gcd.param_buf;
    gate_call(gate, 0, 0, 0).call(&gcd, 0);

}
