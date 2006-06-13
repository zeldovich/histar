extern "C" {
#include <inc/types.h>
#include <inc/syscall.h>
#include <inc/gateparam.h>   
#include <inc/error.h>
#include <inc/debug.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
}

#include <inc/dis/segclient.hh>
#include <inc/dis/exportd.hh>
#include <inc/dis/globallabel.hh>

#include <inc/dis/globalcatc.hh>

#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/gatesrv.hh>
#include <inc/scopeguard.hh>
#include <inc/pthread.hh>
#include <inc/gateclnt.hh>

#include <inc/dis/segmessage.hh>

static char      secure_init;
static struct {
    char host[32];    
    uint16_t port;
    int s;
} server;


static const char man_dbg = 1;
static const char client_dbg = 1;
static const char init_dbg = 1;
static const char msg_dbg = 1;

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
net_client_init(void)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server.port);

    uint32_t ip;
    if ((ip = inet_addr(server.host)) != INADDR_NONE)
        addr.sin_addr.s_addr = ip;
    else {  
        struct hostent *hostent;
        if ((hostent = gethostbyname(server.host)) == 0)
            throw error(-E_INVAL, "unable to resolve %s", server.host);
        memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length) ;
    }
    
    error_check(server.s = socket(AF_INET, SOCK_STREAM, 0));       
    error_check(connect(server.s, (struct sockaddr *)&addr, sizeof(addr)));
}

static void
net_client_read(import_client_arg *ic_arg)
{
    const char *path = ic_arg->path;
    uint32_t count = ic_arg->segment_read.count;
    uint32_t offset = ic_arg->segment_read.offset;
   
    struct cobj_ref seg;
    void *buffer = 0;
    error_check(segment_alloc(start_env->shared_container, count, &seg, &buffer,
                0, "import read buf"));
    scope_guard<int, void *> unmap_va(segment_unmap, buffer);

    segserver_hdr msg;
    msg.op = segserver_read;
    msg.count = count;
    msg.offset = offset;
    strcpy(msg.path, path);
    debug_print(msg_dbg, "count %d off %d path %s", 
                msg.count, msg.offset, msg.path);
    
    // XXX
    error_check(write(server.s, &msg, sizeof(msg)) - sizeof(msg));

    segclient_hdr res;
    // XXX
    error_check(read(server.s, &res, sizeof(res)) - sizeof(res));
    if (res.status < 0)
        throw basic_exception("seg_client::frame_at: remote read failed");
    
    char gl_buf[128];
    
    error_check(read(server.s, gl_buf, res.glsize) - res.glsize);
    error_check(read(server.s, buffer, res.psize - res.glsize) - 
                (res.psize - res.glsize));

    // apply local security policy
    global_label *gl = new global_label(gl_buf);
    global_catc gc;
    label *fl = gc.foreign_label(gl);
    debug_print(msg_dbg, "global label %s", gl->string_rep());
    debug_print(msg_dbg, "foreign label %s", fl->to_string());

    uint64_t tid;
    error_check(tid = sys_segment_copy(seg, start_env->shared_container,
                              fl->to_ulabel(), "tainted seg"));
    delete fl;
    delete gl;
    
    //ic_arg->segment_read.seg = seg;
    ic_arg->segment_read.seg = COBJ(start_env->shared_container, tid);

    ic_arg->status = res.psize - res.glsize;
}

static void
net_client_write(import_client_arg *ic_arg)
{
    cobj_ref seg = ic_arg->segment_write.seg;
    void *va = 0;
    error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    
    int count = ic_arg->segment_write.count;
    int offset = ic_arg->segment_write.offset;
  
    segserver_hdr msg;
    msg.op = segserver_write;
    msg.count = count;
    msg.offset = offset;
    strcpy(msg.path, ic_arg->path);
    debug_print(msg_dbg, "count %d off %d path %s", 
                msg.count, msg.offset, msg.path);
    
    // XXX
    error_check(write(server.s, &msg, sizeof(msg)) - sizeof(msg));
    error_check(write(server.s, va, count) - count);

    segclient_hdr res;
    // XXX
    error_check(read(server.s, &res, sizeof(res)) - sizeof(res));
    ic_arg->status = res.status;
}

static void
net_client_stat(import_client_arg *ic_arg)
{
    struct seg_stat *stat = &ic_arg->segment_stat.stat;
    
    segserver_hdr msg;
    msg.op = segserver_stat;
    strcpy(msg.path, ic_arg->path);
    debug_print(msg_dbg, "path %s", msg.path);
    
    // XXX
    error_check(write(server.s, &msg, sizeof(msg)) - sizeof(msg));

    segclient_hdr res;
    // XXX
    error_check(read(server.s, &res, sizeof(res)) - sizeof(res));
    int cc = MIN(res.psize, sizeof(*stat));
    if (res.psize != sizeof(*stat))
        printf("unexpected stat len %d, %ld\n", res.psize, sizeof(*stat));
    // XXX
    error_check(read(server.s, stat, cc) - cc);    
    ic_arg->status = res.status;
}

static void __attribute__((noreturn))
wrap_gate(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
{
    struct gate_call_data bck;
    memcpy(&bck, parm, sizeof(bck));
    import_client_arg *ic_arg = (import_client_arg*)parm->param_buf;
   
    try { 
        if (!secure_init) {
            debug_print(init_dbg, "init secure channel...");
            net_client_init();
            debug_print(init_dbg, "init secure channel done!");
            secure_init = 1;
            memcpy(parm, &bck, sizeof(*parm));
        }
        
        switch (ic_arg->op) {
            case ic_segment_read:
                net_client_read(ic_arg);
                break;
            case ic_segment_write:
                net_client_write(ic_arg);
                break;
            case ic_segment_stat:
                net_client_stat(ic_arg);
                break;
        }
    }
    catch (basic_exception e) {            
        printf("wrap_gate: %s\n", e.what());
        ic_arg->status = -1;
    }
    memcpy(parm, &bck, sizeof(*parm) - sizeof(parm->param_buf));
    gr->ret(0, 0, 0);    
}

////////////////////////
// export manager gate
////////////////////////

void
import_manager_new_wrap(const char *name, label *lab)
{
    uint64_t ct;
    // container
    label ct_l(1);
    ct_l.set(start_env->process_grant, 0);
    error_check(ct = sys_container_alloc(start_env->shared_container,
                                        ct_l.to_ulabel(), name,
                                        0, CT_QUOTA_INF));
    // gates
    label l;
    thread_cur_label(&l);
    if (lab)
        panic("not implemented");
    label c(2);
    gate_create(ct, "gate", &l, &c, &wrap_gate, 0);    
    debug_print(man_dbg, "created wrap %s", name);
}

static void __attribute__((noreturn))
export_manager(void *arg, struct gate_call_data *vol, gatesrv_return *gr)
{
    struct gate_call_data parm;
    memcpy(&parm, vol, sizeof(parm));
    export_manager_arg *em_arg = (export_manager_arg*)parm.param_buf;
    try {
        switch(em_arg->op) {
            case em_new_iseg:
                break;
            case em_del_iseg:
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

int 
main(int ac, char **av)
{
    if (ac < 2) {
        const char def_host[] = "127.0.0.1";
        printf("%s using default host %s\n", av[0], def_host);
        strcpy(server.host, def_host);
    }
    else
        strcpy(server.host, av[1]);
    
    if (ac < 3) {
        const uint16_t def_port = 9999;
        printf("%s using default port %d\n", av[0], def_port);
        server.port = def_port;
    }
    else 
        server.port = atoi(av[2]);
    server.s = 0;
    
    // manager gate
    label th_l, th_cl;
    thread_cur_label(&th_l);
    thread_cur_clearance(&th_cl);
    gate_create(start_env->shared_container,"manager", &th_l, 
                    &th_cl, &export_manager, 0);
    
    // create a default wrap gate
    secure_init = 0;
    import_manager_new_wrap("default_wrap", 0);
    
    thread_halt();
    return 0;    
}
