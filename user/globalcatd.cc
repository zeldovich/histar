extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/dis/globallabel.hh>
#include <inc/dis/globalcatd.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#define NUM_MAPPINGS 16
struct {
    struct global_cat global;
    uint64_t foreign;
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

/*
static void
add_mapping(gcd_arg *arg)
{
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (!mapping[i].foreign) {
            mapping[i].global.k = arg->global.k;
            mapping[i].global.orignal = arg->global.original;
            mapping[i].foreign = arg->local;

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
        if (mapping[i].foreign == local) {
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
            arg->local = mapping[i].foreign;
            arg->grant_gt = mapping[i].grant_gt;
            arg->status = 0;
        }
    }
    arg->status = -1;    
}
*/

static uint64_t
add_mapping(global_cat *global)
{

    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (!mapping[i].foreign) {
            char buffer[32];

            mapping[i].global.k = global->k;
            mapping[i].global.original = global->original;
            
            uint64_t h = handle_alloc();
            mapping[i].foreign = h;

            // make a gate
            label th_l, th_cl;
            thread_cur_label(&th_l);
            thread_cur_clearance(&th_cl);
            th_l.set(h, LB_LEVEL_STAR);
            th_cl.set(start_env->process_grant, 0);

            sprintf(buffer, "%ld", h);

            cobj_ref gt = gate_create(start_env->shared_container, buffer, &th_l, 
                             &th_cl, &grantcat, (void *)h);
            mapping[i].grant_gt = gt;
            return h;                
        }
    }
}

static uint64_t
g2f(gcd_arg *arg)
{
    for (int i = 0; i < NUM_MAPPINGS; i++) {
        if (mapping[i].global.k == arg->f2g.global.k &&
            mapping[i].global.original == arg->f2g.global.original) {
            
            gate_call_data gcd;
            gate_call(mapping[i].grant_gt, 0, 0, 0).call(&gcd, 0);
                        
            return mapping[i].foreign;
        }
    }        
    return add_mapping(&arg->f2g.global);
}

static void __attribute__((noreturn))
globalcatd(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gate_call_data bck;
    memcpy(&bck, parm, sizeof(bck));

    gcd_arg *args = (gcd_arg *) parm->param_buf;
    switch (args->op) {
        case gcd_g2f: {
            uint64_t f = g2f(args);
            label *dl = new label(3);
            dl->set(f, LB_LEVEL_STAR);

            label th_l;
            thread_cur_label(&th_l);

            memcpy(parm, &bck, sizeof(*parm));
            args->status = 0;
            args->f2g.foreign = f;            
            
            gr->ret(0, dl, 0);        
            break;    
        }
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
