extern "C" {
#include <stdio.h>    
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/error.h>
#include <string.h>
}

#include <lib/dis/proxydsrv.hh>

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/pthread.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

static const char server_taint = 0;

static uint64_t mappings_ct = 0;

struct global_handle {
    uint64_t handle;
    char global[PROX_GLOBAL_LEN];    
};

static const int num_global_handle = 16;
static struct {
    pthread_mutex_t mu;
    global_handle mapping[num_global_handle];
} global_handle_map;

static void __attribute__((noreturn))
get_local(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    proxyd_args *args = (proxyd_args *) parm->param_buf;
    uint64_t local = args->handle.local;
    
    label *ds = new label(3);
    ds->set(local, LB_LEVEL_STAR);
    gr->ret(0, ds, 0);
}

static uint64_t
global_to_local(char *global)
{
    uint64_t mappings_gt;
    error_check(mappings_gt = container_find(mappings_ct, kobj_gate, global));
    
    gate_call_data gcd;
    proxyd_args *args = (proxyd_args *) gcd.param_buf;
    strcpy(args->handle.global, global);
    args->handle.local = 0;

    pthread_mutex_lock(&global_handle_map.mu); 
    global_handle *mapping = global_handle_map.mapping;
    for (int i = 0; i < num_global_handle; i++) {
       if (mapping[i].handle != 0 && 
       !strcmp(mapping[i].global, global)) {
            args->handle.local = mapping[i].handle;
            break;
        }
    }
    pthread_mutex_unlock(&global_handle_map.mu);
    if (!args->handle.local)
        throw error(-E_INVAL, "no mapping for %s", global);    
    

    label th_cl;
    thread_cur_label(&th_cl);
    gate_call(COBJ(mappings_ct, mappings_gt), 0, &th_cl, 0).call(&gcd, 0);

    return args->handle.local;    
}

static char*
local_to_global(uint64_t local)
{
    scoped_pthread_lock(&global_handle_map.mu);    
    global_handle *mapping = global_handle_map.mapping;
    for (int i = 0; i < num_global_handle; i++) {
       if (mapping[i].handle == local) {
            uint64_t mappings_gt;
            error_check(mappings_gt = container_find(mappings_ct, kobj_gate, 
                        mapping[i].global));
            gate_call_data gcd;
            proxyd_args *args = (proxyd_args *) gcd.param_buf;
            args->handle.local = local;
            
            label th_cl;
            thread_cur_label(&th_cl);
            
            gate_call(COBJ(mappings_ct, mappings_gt), 0, &th_cl, 0).call(&gcd, 0);
            return mapping[i].global;
       }
    }
    return 0;
}

static void
add_mapping(char *global, uint64_t local, 
                  uint64_t grant, uint8_t grant_level)
{
    label l(1);
    l.set(local, LB_LEVEL_STAR);
    label c(2);
    c.set(grant, grant_level);
    c.set(start_env->process_grant, 0);
    
    cobj_ref r = gate_create(mappings_ct, global, &l, 
                             &c, &get_local, 0);
    
    scoped_pthread_lock(&global_handle_map.mu);    
    global_handle *mapping = global_handle_map.mapping;                             
    for (int i = 0; i < num_global_handle; i++) {
        if (mapping[i].handle == 0) {
            strcpy(mapping[i].global, global);
            mapping[i].handle = local;
            return;    
        }
    }
    throw error(-E_NO_SPACE, "no room in global_handle");
}

static void __attribute__((noreturn))
proxyd_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    proxyd_args *args = (proxyd_args *) parm->param_buf;
    label *cont = new label(LB_LEVEL_STAR);
    if (server_taint)
        cont->set(start_env->process_taint, 2);
    
    try {
        switch (args->op) {
            case proxyd_mapping:
                add_mapping(args->mapping.global, args->mapping.local, 
                            args->mapping.grant, args->mapping.grant_level);
                break;
            case proxyd_local: {
                label l, cl;
                thread_cur_label(&l);
                thread_cur_clearance(&cl);
                uint64_t local = global_to_local(args->handle.global);
                args->handle.local = local;
                label *ds = new label(3);
                ds->set(local, LB_LEVEL_STAR);
                args->ret = 0;
                gr->ret(cont, ds, 0);        
            }
            case proxyd_global: {
                char *global = local_to_global(args->handle.local);
                if (!global) {
                    args->ret = -1;
                    gr->ret(cont, 0, 0);                           
                }
                strcpy(args->handle.global, global);
                label *ds = new label(3);
                ds->set(args->handle.local, LB_LEVEL_STAR);
                args->ret = 0;
                gr->ret(cont, ds, 0);       
                break;    
            }
            
        }
        args->ret = 0;
        gr->ret(cont, 0, 0);    
    } catch (basic_exception e) {
        printf("proxyd_srv: %s", e.what());
        args->ret = -1;
        gr->ret(0, 0, 0);    
    }
}

struct 
cobj_ref proxydsrv_create(uint64_t container, const char *name,
                label *label, label *clearance)
{
    memset(&global_handle_map, 0, sizeof(global_handle_map));

    int64_t ct;
    error_check(ct = sys_container_alloc(container,
                     label->to_ulabel(), "mappings ct",
                     0, CT_QUOTA_INF));
    mappings_ct = ct;
    
    cobj_ref r = gate_create(container,"proxyd server", label, 
                    clearance, &proxyd_srv, 0);
    return r;
}
