extern "C" {
#include <stdio.h>    
#include <inc/gateparam.h>
#include <inc/syscall.h>
#include <inc/remfile.h>
#include <inc/error.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
}

#include <inc/labelutil.hh>
#include <inc/gateclnt.hh>
#include <inc/gatesrv.hh>
#include <inc/error.hh>

#include <lib/dis/remfiledsrv.hh>
#include <lib/dis/fileclient.hh>

struct remfile_data {
    struct sockaddr_in addr;
    int socket;
    char path[32];    
};

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

    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    if (!data->socket) {
        error_check(data->socket = fileclient_socket());
        error_check(fileclient_connect(data->socket, &data->addr));
    }
    segment_unmap(data);    

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
    
    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    if (!data->socket) {
        error_check(data->socket = fileclient_socket());
        error_check(fileclient_connect(data->socket, &data->addr));
    }
    segment_unmap(data);    

    // XXX
    args->count = args->count;
}

static void
remote_open(remfiled_args *args)
{
    struct cobj_ref seg;
    remfile_data *data = 0;
    error_check(segment_alloc(start_env->proc_container, sizeof(*data),
                             &seg, (void **)&data, 0, "remfile data"));

    strcpy(data->path, args->path);
    error_check(fileclient_addr(args->host, args->port, &data->addr));
    data->socket = 0;
    
    segment_unmap(data);
    args->ino.seg = seg;
}

static void __attribute__((noreturn))
remfiled_srv(void *arg, struct gate_call_data *parm, gatesrv_return *gr)
try {
    remfiled_args *args = (remfiled_args *) parm->param_buf;
    switch(args->op) {
        case rf_read:
            remote_read(args);
            break;
        case rf_write:
            remote_write(args);
            break;
        case rf_open:            
            remote_open(args);
            break;
    }
    gr->ret(0, 0, 0);
}
catch (std::exception &e) {
    printf("proxyd: %s\n", e.what());
    gr->ret(0, 0, 0);
}

struct 
cobj_ref remfiledsrv_create(uint64_t container, label *la, 
                            label *clearance)
{
    cobj_ref r = gate_create(container,"remfiled server", la, 
                    clearance, &remfiled_srv, 0);
    fileclient_socket();
    return r;
}
