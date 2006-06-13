extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

#include <inc/dis/globallabel.hh>
#include <inc/dis/globalcatc.hh>
#include <inc/dis/globalcatd.hh>

#include <inc/cpplabel.hh>

#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

global_catc::global_catc(void)
{
    grant_ = 0;
    int64_t gcd_ct, gcd_gt;
    error_check(gcd_ct = container_find(start_env->root_container, kobj_container, "globalcatd"));
    error_check(gcd_gt = container_find(gcd_ct, kobj_gate, "globalcat srv"));    
    
    gate_ = COBJ(gcd_ct, gcd_gt);
}

global_catc::global_catc(uint64_t grant)
{
    grant_ = grant;
    int64_t gcd_ct, gcd_gt;
    error_check(gcd_ct = container_find(start_env->root_container, kobj_container, "globalcatd"));
    error_check(gcd_gt = container_find(gcd_ct, kobj_gate, "globalcat srv"));    
    
    gate_ = COBJ(gcd_ct, gcd_gt);
}

uint64_t
global_catc::foreign(struct global_cat global)
{
    gate_call_data gcd;
    gcd_arg *arg = (gcd_arg *) gcd.param_buf;

    arg->op = gcd_g2f;
    arg->f2g.global.k = global.k;
    arg->f2g.global.original = global.original;
    label dl(3);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    
    if (arg->status < 0)
        throw basic_exception("unable to get foreign");
    
    return arg->f2g.foreign;
}

label*
global_catc::foreign_label(global_label *gl)
{
    const global_entry *ge= gl->entries();
    uint32_t n = gl->entries_count();
    
    label *ret = new label(1);
    
    for (uint32_t i = 0; i < n; i++) {
        uint64_t f = foreign(ge[i].global);
        ret->set(f, ge[i].level);
    }

    return ret;
}

/*
void
global_catc::global_is(uint64_t h, const char *global)
{
    gate_call_data gcd;
    gcd_arg *arg = (gcd_arg *) gcd.param_buf;

    if (!grant_)
        throw basic_exception("global_catc::global_is: no grant");

    if (global) {
        arg->op = gcd_add;
        strncpy(arg->global, global, sizeof(arg->global) - 1);
        arg->local = h;
        arg->clear = grant_;
        label dl(3);
        dl.set(h, LB_LEVEL_STAR);
        gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    } else {
        throw basic_exception("remove mapping is not implemented");
    }
}

uint64_t
global_catc::local(const char *global, bool grant)
{
    gate_call_data gcd;
    gcd_arg *arg = (gcd_arg *) gcd.param_buf;

    arg->op = gcd_to_local;
    strncpy(arg->global, global, sizeof(arg->global) - 1);
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);
    
    if (arg->status < -1)
        throw basic_exception("global_catc::local: unable to get local for %s", global);        
    
    if (grant) {
        label th_l;
        thread_cur_label(&th_l);
        gate_call(arg->grant_gt, 0, &th_l, 0).call(&gcd, 0);            
    }
    
    return arg->local;
}

void
global_catc::global(uint64_t local, char *global, bool grant)
{
    gate_call_data gcd;
    gcd_arg *arg = (gcd_arg *) gcd.param_buf;

    arg->op = gcd_to_global;
    arg->local = local;
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);
    
    if (arg->status < -1)
        throw basic_exception("global_catc::local: unable to get global for %ld", local);        
    
    if (grant) {
        label th_l;
        thread_cur_label(&th_l);
        gate_call(arg->grant_gt, 0, &th_l, 0).call(&gcd, 0);            
    }
    if (global)
        strcpy(global, arg->global);
}
*/
