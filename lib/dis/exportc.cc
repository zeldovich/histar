extern "C" {
#include <inc/types.h> 
#include <inc/lib.h>   
#include <inc/syscall.h>
#include <inc/gateparam.h>   

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
}

#include <inc/dis/exportd.hh>
#include <inc/dis/exportclient.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

// Client

export_clientc
export_managerc::client_new(char *name, uint64_t grant)
{
    gate_call_data gcd;
    export_manager_arg *arg = (export_manager_arg *) gcd.param_buf;

    arg->op = em_add_client;
    strncpy(arg->user_name, name, sizeof(arg->user_name));
    arg->user_grant = grant;
    label dl(3);
    dl.set(grant, LB_LEVEL_STAR);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to alloc client %s", name);

    return export_clientc(arg->client_gate, arg->client_id, grant);
}

export_segmentc 
export_clientc::segment_new(const char *host, uint16_t port, const char *path)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    arg->op = ec_segment_new;
    strncpy(arg->segment_new.host, host, sizeof(arg->segment_new.host));
    arg->segment_new.port = port;
    strncpy(arg->segment_new.path, path, sizeof(arg->segment_new.path));
    
    label dl(3);
    dl.set(grant_, 0);

    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to alloc %s:%d:%s", host, port, path);

    return export_segmentc(gate_, arg->segment_new.remote_seg, grant_);
}

int
export_segmentc::read(void *buf, int count, int offset)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    uint64_t taint = handle_alloc();
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);

    arg->op = ec_segment_read;
    arg->segment_read.count = count;
    arg->segment_read.offset = offset;
    arg->segment_read.taint = taint;
    arg->segment_read.remote_seg = remote_seg_;
    
    label dl(3);
    dl.set(grant_, 0);
    dl.set(taint, LB_LEVEL_STAR);
    
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to read %d, %d", count ,offset);

    if (arg->status > 0) {
        void *va = 0;
        cobj_ref seg = arg->segment_read.seg;
        error_check(segment_map(seg, SEGMAP_READ, &va, 0));
        memcpy(buf, va, arg->status);
        segment_unmap(va);
        sys_obj_unref(seg);
    }

    return arg->status;
}

int
export_segmentc::write(const void *buf, int count, int offset)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    uint64_t taint = handle_alloc();
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);

    arg->op = ec_segment_write;
    arg->segment_write.count = count;
    arg->segment_write.offset = offset;
    arg->segment_write.taint = taint;
    arg->segment_write.remote_seg = remote_seg_;
 
    struct cobj_ref seg;
    label l(1);
    l.set(start_env->process_grant, 0);
    l.set(taint, 3);
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, count, &seg, &va,
                l.to_ulabel(), "remfiled write buf"));
    memcpy(va, buf, count);
    segment_unmap(va);
    arg->segment_write.seg = seg;

    label dl(3);
    dl.set(grant_, 0);
    dl.set(taint, LB_LEVEL_STAR);
    
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    
    sys_obj_unref(seg);
    return arg->status;    
}
    
void
export_segmentc::stat(struct seg_stat *buf)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    arg->op = ec_segment_stat;
    arg->segment_write.remote_seg = remote_seg_;
    uint64_t taint = handle_alloc();
    arg->segment_stat.taint = taint;
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);
    label dl(1);
    thread_cur_label(&dl);
    dl.set(taint, LB_LEVEL_STAR);
 
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (!arg->status) {
        void *va = 0;
        cobj_ref seg = arg->segment_stat.seg;
        error_check(segment_map(seg, SEGMAP_READ, &va, 0));
        memcpy(buf, va, sizeof(*buf));
        segment_unmap(va);
        sys_obj_unref(seg);
    } else {
        throw basic_exception("export_segmentc::stat unable\n");        
    }
}
