extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/dis/globalcatd.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#define NUM_MAPPINGS 16
struct {
    char global[16];
    uint64_t local;
    cobj_ref grant_gt;    
} mapping[NUM_MAPPINGS];

static void __attribute__((noreturn))
grantcat(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    uint64_t local = (uint64_t) arg;
    label *dl = new label(3);
    dl->set(local, LB_LEVEL_STAR);
    gr->ret(0, dl, 0);        
}

static void
add_mapping(gcd_arg *arg)
{
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (!mapping[i].local) {
            mapping[i].local = arg->local;
            strncpy(mapping[i].global, arg->global, sizeof(mapping[i].global) - 1);
            // make a gate
            label th_l, th_cl;
            thread_cur_label(&th_l);
            thread_cur_clearance(&th_cl);
            th_l.set(arg->local, LB_LEVEL_STAR);
            th_cl.set(arg->clear, 0);
            cobj_ref gt = gate_create(start_env->shared_container,arg->global, &th_l, 
                             &th_cl, &grantcat, (void *)arg->local);
            mapping[i].grant_gt = gt;
            arg->status = 0;
            return;                
        }
    }
}

static void
rem_mapping(gcd_arg *args)
{
    ;    
}

static void
to_global(gcd_arg *arg)
{
    uint64_t local = arg->local;
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (mapping[i].local == local) {
            strncpy(arg->global, mapping[i].global, sizeof(arg->global) - 1);
            arg->grant_gt = mapping[i].grant_gt;
            arg->status = 0;
        }
    }
    arg->status = -1;
}

static void
to_local(gcd_arg *arg)
{
    const char *global = arg->global;
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (!strcmp(global, mapping[i].global)) {
            arg->local = mapping[i].local;
            arg->grant_gt = mapping[i].grant_gt;
            arg->status = 0;
        }
    }
    arg->status = -1;    
}

static void __attribute__((noreturn))
globalcatd(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gcd_arg *args = (gcd_arg *) parm->param_buf;
    switch (args->op) {
        case gcd_add:
            add_mapping(args);
            break;    
        case gcd_rem:
            rem_mapping(args);
            break;
        case gcd_to_global:
            to_global(args);
            break;
        case gcd_to_local:
            to_local(args);
            break;
    }
    gr->ret(0, 0, 0);    
}


int
main (int ac, char **av)
{
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);

    gate_create(start_env->shared_container,"globalcat srv", &th_l, 
                &th_cl, &globalcatd, 0);
    memset(mapping, 0, sizeof(mapping));

    thread_halt();
    return 0;    
}
