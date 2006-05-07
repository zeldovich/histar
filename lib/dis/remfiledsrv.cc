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

#include <lib/dis/remfiledsrv.hh>
#include <lib/dis/fileclient.hh>

static const char srv_debug = 1;

struct remfile_data {
    fileclient fc;
};

static void
remote_read(remfiled_args *args)
{
    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, data);

    struct cobj_ref seg;
    label l(1);
    l.set(args->grant, 0);
    l.set(args->taint, 3);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, args->count, &seg, &va,
                l.to_ulabel(), "remfiled read buf"));
    scope_guard<int, void *> unmap_va(segment_unmap, va);

    const file_frame *frame = data->fc.frame_at(args->count, args->off);
    memcpy(va, frame->byte_, frame->count_);

    args->seg = seg;
    args->count = frame->count_;
}

static void
remote_write(remfiled_args *args)
{
    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, data);
    void *va = 0;
    error_check(segment_map(args->seg, SEGMAP_READ, &va, 0));
    scope_guard<int, void *> unmap_va(segment_unmap, va);
    
    const file_frame *frame = data->fc.frame_at_is(va, args->count, args->off);
    args->count = frame->count_;
}

static void
remote_open(remfiled_args *args)
{
    struct cobj_ref seg;
    remfile_data *data = 0;
    error_check(segment_alloc(start_env->proc_container, sizeof(*data),
                             &seg, (void **)&data, 0, "remfile data"));
    scope_guard<int, void *> unmap_data(segment_unmap, data);

    data->fc.init(args->path, args->host, args->port);
    args->ino.seg = seg;
}

static void
remote_stat(remfiled_args *args)
{
    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, data);
    
    struct cobj_ref seg;
    label l(1);
    l.set(args->grant, 0);
    l.set(args->taint, 3);
    file_stat *fs = 0;
    error_check(segment_alloc(start_env->shared_container, sizeof(*fs),
                &seg, (void **)&fs, l.to_ulabel(), "remfiled stat buf"));
    args->count = data->fc.stat(fs);
    scope_guard<int, void *> unmap_va(segment_unmap, fs);
    args->seg = seg;
}

static void
remote_close(remfiled_args *args)
{
    remfile_data *data = 0;
    error_check(segment_map(args->ino.seg, SEGMAP_READ|SEGMAP_WRITE, (void **)&data, 0));
    scope_guard<int, void *> unmap_data(segment_unmap, data);
    
    data->fc.destroy();
    sys_obj_unref(args->ino.seg);
    args->count = 0;
    return;    
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
        case rf_stat:
            remote_stat(args);
            break;
        case rf_close:
            remote_close(args);
            break;            
    }
    gr->ret(0, 0, 0);
}
catch (std::exception &e) {
    printf("remfiled_srv: %s\n", e.what());
    gr->ret(0, 0, 0);
}

struct 
cobj_ref remfiledsrv_create(uint64_t container, label *la, 
                            label *clearance)
{
    cobj_ref r = gate_create(container,"remfiled server", la, 
                    clearance, &remfiled_srv, 0);
    return r;
}
