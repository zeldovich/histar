extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

#include <inc/dis/catc.hh>
#include <inc/dis/catd.hh>

#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

catc::catc(void)
{
    int64_t gcd_ct, gcd_gt;
    error_check(gcd_ct = container_find(start_env->root_container, kobj_container, "catd"));
    error_check(gcd_gt = container_find(gcd_ct, kobj_gate, "catd gate"));    
    
    grant_ = 0;
    gate_ = COBJ(gcd_ct, gcd_gt);
}

catc::catc(uint64_t grant)
{
    int64_t gcd_ct, gcd_gt;
    error_check(gcd_ct = container_find(start_env->root_container, kobj_container, "catd"));
    error_check(gcd_gt = container_find(gcd_ct, kobj_gate, "catd gate"));    
    
    grant_ = grant;
    gate_ = COBJ(gcd_ct, gcd_gt);
}

void
catc::grant_cat(uint64_t h)
{
    gate_call_data gcd;
    cd_arg *arg = (cd_arg *) gcd.param_buf;

    arg->op = cd_add;
    arg->add.local = h;
    label dl(3);
    dl.set(h, LB_LEVEL_STAR);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
}

cobj_ref
catc::package(const char *path)
{
    gate_call_data gcd;
    cd_arg *arg = (cd_arg *) gcd.param_buf;

    arg->op = cd_package;
    strcpy(arg->package.path, path);
    arg->package.cipher_ct = start_env->shared_container;
    label dl(3);
    dl.set(start_env->process_grant, LB_LEVEL_STAR);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to package %s", path);
    
    return arg->package.seg; 
}

int
catc::write(const char *path, void *buffer, int len, int off)
{
    gate_call_data gcd;
    cd_arg *arg = (cd_arg *) gcd.param_buf;

    arg->op = cd_write;
    strcpy(arg->write.path, path);
    arg->write.len = len;
    arg->write.off = off;

    cobj_ref seg;
    label l(1);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, len, &seg, &va,
                l.to_ulabel(), "catc write buf"));
    memcpy(va, buffer, len);
    segment_unmap(va);
    arg->write.seg = seg;
    
    label dl(3);
    dl.set(start_env->process_grant, LB_LEVEL_STAR);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to package %s", path);

    sys_obj_unref(seg);    
    return arg->status; 
}

/*
uint64_t
catc::local(const char *global, bool grant)
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
catc::global(uint64_t local, char *global, bool grant)
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

