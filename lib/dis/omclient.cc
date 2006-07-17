extern "C" {
#include <inc/lib.h>
#include <inc/gateparam.h>

#include <string.h>


#include <inc/dis/omd.h>
}

#include <inc/dis/omclient.hh>

#include <inc/error.hh>
#include <inc/gateclnt.hh>

om_client::om_client(const char *name)
{
    name_ = strdup(name);
}

om_client::~om_client(void)
{
    delete name_;
}

bool
om_client::observable(om_res *res, palid k) 
{
    return om_test(res, k, true);
}

bool
om_client::modifiable(om_res *res, palid k) 
{
    return om_test(res, k, false);
}

cobj_ref
om_client::om_gate(void) const
{
    int64_t ct, gt;
    error_check(ct = container_find(start_env->root_container, 
				    kobj_container, name_));
    error_check(gt = container_find(ct, kobj_gate, "om gate"));    
    return COBJ(ct, gt);
}

int
om_client::om_test(om_res *res, palid k, bool observe)
{
    cobj_ref gate = om_gate();
    gate_call_data gcd;
    om_args *args = (om_args*)gcd.param_buf;
    
    args->op = observe ? om_observe : om_modify;
    args->observe.t = 1;
    memcpy(&args->observe.res, res, sizeof(*res));
    
    gate_call(gate, 0, 0, 0).call(&gcd, 0);
    
    return args->ret;
}
