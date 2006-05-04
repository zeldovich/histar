extern "C" {
#include <stdio.h>    
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/remfile.h>
#include <inc/error.h>
#include <string.h>
#include <stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#include <lib/dis/remfiledsrv.hh>


static void
remote_read(remfiled_args *args)
{
    struct cobj_ref seg;
    label l(1);
    l.set(args->grant, 0);
    l.set(args->taint, 3);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, args->count, &seg, &va,
                l.to_ulabel(), "remfiled read buf"));
    memset(va, 1, args->count);
    segment_unmap(va);

    args->seg = seg;
    // XXX
    args->count = args->count;
}

static void
remote_write(remfiled_args *args)
{
    void *va = 0;
    error_check(segment_map(args->seg, SEGMAP_READ, &va, 0));
    segment_unmap(va);
    
    // XXX
    args->count = args->count;
}

static void __attribute__((noreturn))
remfiled_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    remfiled_args *args = (remfiled_args *) parm->param_buf;
    
    switch(args->op) {
        case remfile_read:
            remote_read(args);
            break;
        case remfile_write:
            remote_write(args);
            break;            
    }
    gr->ret(0, 0, 0);
}

struct 
cobj_ref remfiledsrv_create(uint64_t container, label *label, 
                            label *clearance)
{
    cobj_ref r = gate_create(container,"remfiled server", label, 
                    clearance, &remfiled_srv, 0);
    
    return r;
}
