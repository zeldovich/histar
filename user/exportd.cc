extern "C" {
#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/gateparam.h>   
#include <inc/error.h>
#include <inc/debug.h>

#include <string.h>
#include <stdio.h>
}

#include <inc/dis/segclient.hh>
#include <inc/dis/exportd.hh>
#include <inc/dis/ca.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/pthread.hh>

static uint64_t  clients_ct;

static const char client_ver = 1;

// XXX
// should live in a segment that is read-only to export clients.
static class  
{
public:
    int alloc(void) {
        scoped_pthread_lock l(&mu_);
        for (int i = 0; i < max_clients; i++)         
            if (!client_[i].inuse) {
                client_[i].inuse = 1;
                return i;    
            }
        throw error(-E_NO_SPACE, "unable to alloc client");
    }
    void free(int id) {
        scoped_pthread_lock l(&mu_);
        if (!client_[id].inuse)
            throw error(-E_INVAL, "client %d not in use", id); 
        client_[id].inuse = 0;      
    }
    void data_is(int id, cobj_ref data) {
        client_[id].data = data;
    }
    cobj_ref data(int id) {
        return client_[id].data;
    }

private:
    static const int max_clients = 16;
    struct {
        char     inuse;
        cobj_ref data;       
    } client_[max_clients];
    pthread_mutex_t mu_;
} client_collection;



struct client_data
{
    uint64_t container;
};

////////////////////////
// export client gate
////////////////////////

static void
seg_client_new(uint64_t container, export_client_arg *arg)
{
    struct cobj_ref seg;
    seg_client *sc = 0;
    error_check(segment_alloc(container, sizeof(*sc),
                             &seg, (void **)&sc, 0, "fc"));
    scope_guard<int, void *> unmap_data(segment_unmap, sc);

    sc->init(arg->segment_new.path, arg->segment_new.host, 
             arg->segment_new.port);
    
    // to access bob's data
    sc->auth_user("bob");
    // to trust server w/ data 
    //sc->auth_server();
    // so the server can trust us w/ tainted data
    //sc->auth_client("client 5");

    arg->status = 0;
    arg->segment_new.remote_seg = seg.object;
    return;    
}

static void
seg_client_read(uint64_t container, export_client_arg *arg)
{
    seg_client *sc = 0;
    error_check(segment_map(COBJ(container, arg->segment_read.remote_seg), 
                0, SEGMAP_READ|SEGMAP_WRITE, (void **)&sc, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, sc);

    int count = arg->segment_read.count;
    int offset = arg->segment_read.offset;
    uint64_t taint = arg->segment_read.taint;
    struct cobj_ref seg;
    label l(1);
    l.set(taint, 3);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, count, &seg, &va,
                l.to_ulabel(), "exportclient read buf"));
    scope_guard<int, void *> unmap_va(segment_unmap, va);

    const seg_frame *frame = sc->frame_at(count, offset);
    debug_print(client_ver, "read %ld from ...", frame->count_);
    memcpy(va, frame->byte_, frame->count_);

    arg->segment_read.seg = seg;
    arg->status = frame->count_;
  
    return;    
}

static void
seg_client_write(uint64_t container, export_client_arg *arg)
{
    seg_client *sc = 0;
    error_check(segment_map(COBJ(container, arg->segment_write.remote_seg), 
                0, SEGMAP_READ|SEGMAP_WRITE, (void **)&sc, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, sc);

    cobj_ref seg = arg->segment_write.seg;
    void *va = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    
    int count = arg->segment_write.count;
    int offset = arg->segment_write.offset;
    const seg_frame *frame = sc->frame_at_is(va, count, offset);
    debug_print(client_ver, "wrote %ld to ...", frame->count_);
    arg->status = frame->count_;
}

static void
seg_client_stat(uint64_t container, export_client_arg *arg)
{

    seg_client *sc = 0;
    error_check(segment_map(COBJ(container, arg->segment_stat.remote_seg), 
                0, SEGMAP_READ|SEGMAP_WRITE, (void **)&sc, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, sc);

    struct cobj_ref seg;
    label l(1);
    l.set(arg->segment_stat.taint, 3);
    seg_stat *ss = 0;
    error_check(segment_alloc(start_env->shared_container, sizeof(*ss),
                &seg, (void **)&ss, l.to_ulabel(), "seg_client stat buf"));
    arg->status = sc->stat(ss);
    scope_guard<int, void *> unmap_va(segment_unmap, ss);
    arg->segment_stat.seg = seg;
    return;    
}

static void __attribute__((noreturn))
export_client(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    export_client_arg *ec_arg = (export_client_arg*)parm->param_buf;
    
    try {
        cobj_ref seg = client_collection.data(ec_arg->id);
        client_data *data;
        error_check(segment_map(seg, 0, SEGMAP_READ|SEGMAP_WRITE, 
                                (void **)&data, 0));
        scope_guard<int, void *> unmap_data(segment_unmap, data);
                                
        switch(ec_arg->op) {
            case ec_segment_new:
                seg_client_new(data->container, ec_arg);
                break;
            case ec_segment_read:
                seg_client_read(data->container, ec_arg);
                break;
            case ec_segment_write:
                seg_client_write(data->container, ec_arg);
                break;
            case ec_segment_stat:
                seg_client_stat(data->container, ec_arg);
                break;
            default:
                break;    
        }
        gr->ret(0, 0, 0);    
    } catch (basic_exception e) {
        printf("export_client: %s\n", e.what());
        ec_arg->status = -1;
        gr->ret(0, 0, 0);    
    }
    gr->ret(0, 0, 0);    
}

////////////////////////
// export manager gate
////////////////////////

void
export_manager_add_client(export_manager_arg *em_arg)
{
    int id = client_collection.alloc();
    try {
        uint64_t taint = handle_alloc();
        // container
        label ct_l(1);
        ct_l.set(taint, 3);
        uint64_t client_ct;    
        error_check(client_ct = sys_container_alloc(clients_ct,
                     ct_l.to_ulabel(), em_arg->user_name,
                     0, CT_QUOTA_INF));
        
        // data segment
        label da_l(1);
        ct_l.set(taint, 3);
        cobj_ref data_seg;
        client_data *data;
        error_check(segment_alloc(client_ct, sizeof(*data), &data_seg, 
                                 (void**)&data, da_l.to_ulabel(), "data"));
        scope_guard<int, void *> unmap_data(segment_unmap, data);
        data->container = client_ct;
        client_collection.data_is(id, data_seg);
    
        // gate
        label l;
        thread_cur_label(&l);
        label c(2);
        c.set(em_arg->user_grant, 0);
        
        em_arg->client_gate = gate_create(clients_ct, "client", &l, 
                                            &c, &export_client, 0);    
        em_arg->client_id = id;
        em_arg->status = 0;
    } catch (basic_exception e) {                      
        client_collection.free(id);
        throw e;
    }
}

static void __attribute__((noreturn))
export_manager(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    export_manager_arg *em_arg = (export_manager_arg*)parm->param_buf;
    try {
        switch(em_arg->op) {
            case em_add_client:
                export_manager_add_client(em_arg);
                break;
            default:
                break;    
        }
        gr->ret(0, 0, 0);    
    } catch (basic_exception e) {
        printf("export_manager: %s\n", e.what());
        em_arg->status = -1;
        gr->ret(0, 0, 0);    
    }
}

void
export_manager_new(void)
{
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    
    gate_create(start_env->shared_container,"manager", &th_l, 
                    &th_cl, &export_manager, 0);
    
    label ct_l(1);
    ct_l.set(start_env->process_grant, 0);
    
    error_check(clients_ct = sys_container_alloc(start_env->shared_container,
                     ct_l.to_ulabel(), "clients",
                     0, CT_QUOTA_INF));
}

int 
main(int ac, char **av)
{
    export_manager_new();
    thread_halt();
    return 0;    
}
