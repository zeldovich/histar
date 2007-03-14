extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
}

#include <inc/segmentutil.hh>
#include <inc/cpplabel.hh>
#include <dj/gatecallstatus.h>

enum { gatecall_incomplete = 0, gatecall_complete };
struct gatecall_status {
    uint64_t signal;
    uint64_t res;
};

cobj_ref
gatecall_status_alloc(uint64_t ct, label *l)
{
    gatecall_status *gcs = 0;
    cobj_ref gcs_obj;
    int r = segment_alloc(ct, sizeof(*gcs), &gcs_obj,
			  (void **)&gcs, l->to_ulabel(), "gatecall status");
    if (r < 0)
	throw error(r, "unable to alloc gatecall status seg");
    
    memset(gcs, 0, sizeof(*gcs));
    segment_unmap_delayed(gcs, 1);
    return gcs_obj;
}

void
gatecall_status_done(cobj_ref gcs_obj, uint64_t res)
{
    segment_writer<gatecall_status> sw(gcs_obj);
    gatecall_status *gcs = sw.addr();
    gcs->res = res;
    gcs->signal = gatecall_complete;
    sys_sync_wakeup(&gcs->signal);
}

uint64_t 
gatecall_status_wait(cobj_ref gcs_obj)
{
    segment_reader<gatecall_status> sr(gcs_obj);
    gatecall_status *gcs = sr.addr();
    int64_t r = sys_sync_wait(&gcs->signal, gatecall_incomplete ,~0UL);
    if (r < 0)
	throw error(r, "unable to wait on signal");
    return gcs->res;
}

