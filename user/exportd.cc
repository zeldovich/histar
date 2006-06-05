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
#include <inc/dis/globallabel.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/pthread.hh>
#include <inc/gateclnt.hh>

static uint64_t  clients_ct;

static const char client_ver = 1;

static const char man_dbg = 0;
static const char clnt_dbg = 0;

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
    void container_is(int id, uint64_t container) {
        client_[id].container = container;
    }
    cobj_ref data(int id) {
        return client_[id].data;
    }
    uint64_t container(int id) {
        return client_[id].container;
    }


private:
    static const int max_clients = 16;
    struct {
        char     inuse;
        uint64_t container; // for deletion
        cobj_ref data;       
    } client_[max_clients];
    pthread_mutex_t mu_;
} client_collection;



struct client_data
{
    seg_client sc;
    cobj_ref net_gate;
    uint64_t user_grant;
};

////////////////////////
// export client gate
////////////////////////

static void
seg_client_new(seg_client *sc, import_client_arg *arg)
{
    sc->init(arg->segment_new.path, arg->segment_new.host, 
             arg->segment_new.port);
    
    // to access bob's data
    //sc->auth_user("bob");
    // to trust server w/ data 
    //sc->auth_server();
    // so the server can trust us w/ tainted data
    //sc->auth_client("client 5");

    const global_label *gl = sc->label();
    struct cobj_ref seg;
    void *va = 0;

    error_check(segment_alloc(start_env->shared_container, gl->serial_len(), &seg, &va,
                0, "global label"));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    memcpy(va, gl->serial(), gl->serial_len());

    arg->segment_new.gl_seg = seg;
    arg->status = 0;
    
    return;    
}

static void
seg_client_read(seg_client *sc, import_client_arg *arg)
{
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
seg_client_write(seg_client *sc, import_client_arg *arg)
{
    cobj_ref seg = arg->segment_write.seg;
    void *va = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    
    int count = arg->segment_write.count;
    int offset = arg->segment_write.offset;
    const seg_frame *frame = sc->frame_at_is(va, count, offset);
    debug_print(client_ver, "wrote %ld to ...", frame->count_);
    arg->status = frame->count_;
}

static void
seg_client_stat(seg_client *sc, import_client_arg *arg)
{
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

static void
seg_client_close(seg_client *sc, import_client_arg *arg)
{
    sc->destroy();
    arg->status = 0;
    return;    
}

static void __attribute__((noreturn))
export_client(void *arg, struct gate_call_data *vol, gatesrv_return *gr)
{
    struct gate_call_data parm;
    memcpy(&parm, vol, sizeof(parm));
    import_client_arg *ec_arg = (import_client_arg*)parm.param_buf;
    
    try {
        cobj_ref seg = client_collection.data(ec_arg->id);
        client_data *data;
        error_check(segment_map(seg, 0, SEGMAP_READ|SEGMAP_WRITE, 
                                (void **)&data, 0, 0));
        scope_guard<int, void *> unmap_data(segment_unmap, data);
        debug_print(clnt_dbg, "(%d) op %d", ec_arg->id, ec_arg->op);
                                
        switch(ec_arg->op) {
            case ic_segment_new:
                seg_client_new(&data->sc, ec_arg);
                break;
            case ic_segment_read:
                seg_client_read(&data->sc, ec_arg);
                break;
            case ic_segment_write:
                seg_client_write(&data->sc, ec_arg);
                break;
            case ic_segment_stat:
                seg_client_stat(&data->sc, ec_arg);
                break;
            case ic_segment_close:
                seg_client_close(&data->sc, ec_arg);
            default:
                break;    
        }
    } catch (basic_exception e) {
        printf("export_client: %s\n", e.what());
        ec_arg->status = -1;
    }
    memcpy(vol, &parm, sizeof(*vol));    
    gr->ret(0, 0, 0);    
}

static void __attribute__((noreturn))
wrap_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct gate_call_data bck;
    memcpy(&bck, parm, sizeof(bck));
    import_client_arg *ec_arg = (import_client_arg*)parm->param_buf;
   
    try { 
        cobj_ref seg = client_collection.data(ec_arg->id);
        client_data *data;
        error_check(segment_map(seg, 0, SEGMAP_READ|SEGMAP_WRITE, 
                                (void **)&data, 0, 0));
        scope_guard<int, void *> unmap_data(segment_unmap, data);
        
        label dl;
        thread_cur_label(&dl);
        //dl.set(data->user_grant, LB_LEVEL_STAR);
        gate_call(data->net_gate, 0, &dl, 0).call(parm, 0);
    }
    catch (basic_exception e) {            
        ec_arg->status = -1;
    }
    memcpy(parm, &bck, sizeof(*parm) - sizeof(parm->param_buf));
    gr->ret(0, 0, 0);    
}

////////////////////////
// export manager gate
////////////////////////

void
export_manager_new_segment(export_manager_arg *em_arg)
{
    gate_call_data gcd;
    import_client_arg *arg = (import_client_arg *) gcd.param_buf;

    int id = client_collection.alloc();
    try {
        uint64_t taint = handle_alloc();
        // container
        label ct_l(1);
        //ct_l.set(taint, 3);
        ct_l.set(em_arg->user_grant, 0);
        uint64_t client_ct;    
        error_check(client_ct = sys_container_alloc(clients_ct,
                     ct_l.to_ulabel(), em_arg->host,
                     0, CT_QUOTA_INF));
        
        // data segment
        label da_l(1);
        ct_l.set(taint, 3);
        cobj_ref data_seg;
        client_data *data;
        error_check(segment_alloc(client_ct, sizeof(*data), &data_seg, 
                                 (void**)&data, da_l.to_ulabel(), "data"));
        scope_guard<int, void *> unmap_data(segment_unmap, data);
        data->user_grant = em_arg->user_grant;
        client_collection.data_is(id, data_seg);
        client_collection.container_is(id, client_ct);

        // gates
        label l;
        thread_cur_label(&l);
        label c(2);
        c.set(em_arg->user_grant, 0);
        c.set(start_env->process_grant, 0);
                
        cobj_ref net_gt, wrap_gt;
                
        net_gt = gate_create(client_ct, "net gate", &l, 
                             &c, &export_client, 0);    
        data->net_gate = net_gt;
        
        arg->op = ic_segment_new;
        arg->id = id;
        strncpy(arg->segment_new.host, em_arg->host, sizeof(arg->segment_new.host) - 1);
        arg->segment_new.port = em_arg->port;
        strncpy(arg->segment_new.path, em_arg->path, sizeof(arg->segment_new.path) - 1);

        label dl(3);
        dl.set(em_arg->user_grant, LB_LEVEL_STAR);
        gate_call(net_gt, 0, &dl, 0).call(&gcd, 0);
        if (arg->status < 0)
            throw basic_exception("export_manager: cannot create net gate");            

        cobj_ref seg = arg->segment_new.gl_seg;
        void *va = 0;
        error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
        global_label gl((const char *)va);
        scope_guard<int, void *> unmap_va(segment_unmap, va);
        scope_guard<int, cobj_ref> unref_seg(sys_obj_unref, seg);

        label l1;
        thread_cur_label(&l1);
        label c1(2);
        c1.set(em_arg->user_grant, 0);

        wrap_gt = gate_create(client_ct, "wrap gate", &l1, 
                              &c1, &wrap_gate, 0);    
        em_arg->client_gate = wrap_gt;

        em_arg->client_id = id;
        em_arg->status = 0;
        debug_print(man_dbg, "(%d) ct %ld, net %ld, wrap %ld", id, 
                    client_ct, net_gt.object, wrap_gt.object);
        
    } catch (basic_exception e) {                      
        client_collection.free(id);
        throw e;
    }
}

void
export_manager_del_segment(export_manager_arg *em_arg)
{
    gate_call_data gcd;
    import_client_arg *arg = (import_client_arg *) gcd.param_buf;
    
    try {
        // gate
        label l;
        thread_cur_label(&l);
        
        arg->op = ic_segment_close;
        arg->id = em_arg->client_id;
    
        gate_call(em_arg->client_gate, 0, &l, 0).call(&gcd, 0);
        // and get rid of objects...
        uint64_t ct = client_collection.container(em_arg->client_id);
        error_check(sys_obj_unref(COBJ(clients_ct, ct)));
        client_collection.free(em_arg->client_id);
        debug_print(man_dbg, "(%d) container %ld", em_arg->client_id, ct);
    } catch (basic_exception e) {                      
        printf("export_manager_del_segment: %s", e.what());
        throw e;
    }
}

static void __attribute__((noreturn))
export_manager(void *arg, struct gate_call_data *vol, gatesrv_return *gr)
{
    struct gate_call_data parm;
    memcpy(&parm, vol, sizeof(parm));
    export_manager_arg *em_arg = (export_manager_arg*)parm.param_buf;
    try {
        switch(em_arg->op) {
            case em_new_segment:
                export_manager_new_segment(em_arg);
                break;
            case em_del_segment:
                export_manager_del_segment(em_arg);
                break;
            default:
                break;    
        }
    } catch (basic_exception e) {
        printf("export_manager: %s\n", e.what());
        em_arg->status = -1;
    }
    memcpy(vol, &parm, sizeof(*vol));
    gr->ret(0, 0, 0);    
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
