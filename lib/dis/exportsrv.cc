extern "C" {
#include <stdio.h>    
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/remfile.h>
#include <inc/error.h>
#include <string.h>
#include <stdio.h>
#include <inc/debug.h>
}

#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#include <lib/dis/fileserver.hh>
#include <lib/dis/proxydclnt.hh>
#include <lib/dis/proxydsrv.hh>

#include <lib/dis/exportsrv.hh>

static uint64_t handles_ct;

static void __attribute__((noreturn))
grant_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
try {
    char buf[PROX_GLOBAL_LEN];
    uint64_t taint = (uint64_t)arg;
    proxyd_get_global(taint, buf);
    label *dl = new label();;
    dl->set(taint, LB_LEVEL_STAR);
    gr->ret(0, dl, 0);
}
catch (std::exception &e) {
    printf("grant_srv: %s\n", e.what());
    gr->ret(0, 0, 0);
}


static void
add_grant_gate(uint64_t grant, uint64_t handle)
{
    label th_ctm, th_clr;
    thread_cur_label(&th_ctm);
    thread_cur_clearance(&th_clr);
    th_ctm.set(grant, LB_LEVEL_STAR);
    
    char name[32];
    sprintf(name, "%ld", handle);
    gate_create(handles_ct, name, &th_ctm, 
                    &th_clr, &grant_srv, (void *)handle);
}

void
export_acquire(uint64_t taint)
{
    char name[32];
    sprintf(name, "%ld", taint);
    int taint_gt;
    error_check(taint_gt = container_find(handles_ct, kobj_gate, name));
    
    gate_call_data gcd;
    gate_call(COBJ(handles_ct, taint_gt), 0, 0, 0).call(&gcd, 0);
}

static void __attribute__((noreturn))
export_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
try {
    export_args *args = (export_args *) parm->param_buf;
    
    switch(args->op) {
        case exp_grant:
            add_grant_gate(args->grant, args->handle);
            break;    
    }
    gr->ret(0, 0, 0);
}
catch (std::exception &e) {
    printf("export_srv: %s\n", e.what());
    gr->ret(0, 0, 0);
}

void 
exportsrv_start(uint64_t container, label *la, 
                label *clearance)
{
    int64_t ct;
    error_check(ct = sys_container_alloc(container,
                     la->to_ulabel(), "handles ct",
                     0, CT_QUOTA_INF));
    handles_ct = ct;

    gate_create(container,"export server", la, 
                    clearance, &export_srv, 0);
    
    fileserver_start(8080);
}

