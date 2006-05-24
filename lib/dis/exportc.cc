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

export_segmentc
export_managerc::segment_new(const char *host, uint16_t port, 
                             const char *path, uint64_t grant)
{
    gate_call_data gcd;
    export_manager_arg *arg = (export_manager_arg *) gcd.param_buf;

    arg->op = em_new_segment;
    strncpy(arg->host, host, sizeof(arg->host));
    arg->port = port;
    strncpy(arg->path, path, sizeof(arg->path));
    arg->user_grant = grant;

    label dl(3);
    dl.set(grant, LB_LEVEL_STAR);

    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);

    if (arg->status < 0)
        throw basic_exception("unable to alloc segment @ %s", host);

    return export_segmentc(arg->client_gate, arg->client_id, grant);
}

void
export_managerc::segment_del(export_segmentc *seg)
{
    gate_call_data gcd;
    export_manager_arg *arg = (export_manager_arg *) gcd.param_buf;

    arg->op = em_del_segment;
    arg->client_id = seg->id();
    arg->client_gate = seg->gate();
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);

    if (arg->status < 0)
        throw basic_exception("unable to close segment");
}

int
export_segmentc::read(void *buf, int count, int offset)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    uint64_t taint = handle_alloc();
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);

    arg->op = ec_segment_read;
    arg->id = id_;
    arg->segment_read.count = count;
    arg->segment_read.offset = offset;
    arg->segment_read.taint = taint;
    
    label dl(3);
    dl.set(grant_, 0);
    dl.set(taint, LB_LEVEL_STAR);
    
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("unable to read %d, %d", count ,offset);

    if (arg->status > 0) {
        void *va = 0;
        cobj_ref seg = arg->segment_read.seg;
        error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
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
    arg->id = id_;
    arg->segment_write.count = count;
    arg->segment_write.offset = offset;
    arg->segment_write.taint = taint;
 
    struct cobj_ref seg;
    label l(1);
    l.set(start_env->process_grant, 0);
    l.set(taint, 3);
    // XXX have cagent taint
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
    arg->id = id_;
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
        error_check(segment_map(seg, 0, SEGMAP_READ, &va, 0, 0));
        memcpy(buf, va, sizeof(*buf));
        segment_unmap(va);
        sys_obj_unref(seg);
    } else {
        throw basic_exception("export_segmentc::stat unable\n");        
    }
}

void
export_segmentc::close(void)
{
    gate_call_data gcd;
    export_client_arg *arg = (export_client_arg *) gcd.param_buf;

    arg->op = ec_segment_close;
    arg->id = id_;
 
    gate_call(gate_, 0, 0, 0).call(&gcd, 0);
    if (arg->status < 0)
        throw basic_exception("export_segmentc::close unable\n");        
}

