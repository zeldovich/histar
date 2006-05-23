extern "C" {
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>

#include <string.h>
}

#include <inc/dis/globalcatd.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

struct {
    char global[16];
    uint64_t local;    
} mapping[16];

static void __attribute__((noreturn))
grantcat(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    
}

static void
add_mapping(gcd_args *args)
{
    ;
}

static void
rem_mapping(gcd_args *args)
{
    ;    
}

static void
to_global(gcd_args *args)
{
    
}

static void
to_local(gcd_args *args)
{
    
}

static void __attribute__((noreturn))
globalcatd(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    gcd_args *args = (gcd_args *) parm->param_buf;
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

    gate_create(start_env->shared_container,"globallabel srv", &th_l, 
                &th_cl, &globalcatd, 0);
    memset(mapping, 0, sizeof(mapping));

    thread_halt();
    return 0;    
}
