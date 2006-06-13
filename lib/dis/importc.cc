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
#include <inc/dis/importc.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/error.hh>
#include <inc/gateclnt.hh>

// Client

import_segmentc*
import_managerc::segment_new(const char *path)
{
    import_segmentc* ret = new import_segmentc(wrap_gt_, path);

    if (!ret)
        throw basic_exception("unable to alloc segment");

    return ret;
}

void
import_managerc::segment_del(import_segmentc *seg)
{
    delete seg;
}

import_segmentc::import_segmentc(cobj_ref gate, const char *path) 
 : gate_(gate)
{
    path_ = strdup(path);    
}

int
import_segmentc::read(void *buf, int count, int offset)
{
    gate_call_data gcd;
    import_client_arg *arg = (import_client_arg *) gcd.param_buf;

    uint64_t taint = handle_alloc();
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);

    arg->op = ic_segment_read;
    strcpy(arg->path, path_);
    arg->segment_read.count = count;
    arg->segment_read.offset = offset;
    
    label dl(3);
    
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
import_segmentc::write(const void *buf, int count, int offset)
{
    gate_call_data gcd;
    import_client_arg *arg = (import_client_arg *) gcd.param_buf;

    uint64_t taint = handle_alloc();
    scope_guard<void, uint64_t> drop_taint(thread_drop_star, taint);

    arg->op = ic_segment_write;
    strcpy(arg->path, path_);
    arg->segment_write.count = count;
    arg->segment_write.offset = offset;
 
    struct cobj_ref seg;
    label l(1);
    // XXX taint
    void *va = 0;
    error_check(segment_alloc(start_env->shared_container, count, &seg, &va,
                l.to_ulabel(), "remfiled write buf"));
    memcpy(va, buf, count);
    segment_unmap(va);
    arg->segment_write.seg = seg;

    label dl(3);
    
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    
    sys_obj_unref(seg);
    return arg->status;    
}
    
void
import_segmentc::stat(struct seg_stat *buf)
{
    gate_call_data gcd;
    import_client_arg *arg = (import_client_arg *) gcd.param_buf;

    arg->op = ic_segment_stat;
    strcpy(arg->path, path_);

    label dl(1);
    gate_call(gate_, 0, &dl, 0).call(&gcd, 0);
    if (!arg->status) {
        memcpy(buf, &arg->segment_stat.stat, sizeof(*buf));
    } else {
        throw basic_exception("import_segmentc::stat unable\n");        
    }
}
