extern "C" {
#include <inc/lib.h>
#include <inc/syscall.h>
}

#include <inc/segmentutil.hh>
#include <inc/cpplabel.hh>
#include <dj/gatecallstatus.hh>

cobj_ref
gatecall_status_alloc(uint64_t ct, label *l)
{
    cobj_ref gcs_obj;
    int r = segment_alloc(ct, sizeof(dj_delivery_code), &gcs_obj, 0,
			  l->to_ulabel(), "gatecall status");
    if (r < 0)
	throw error(r, "unable to alloc gatecall status seg");

    return gcs_obj;
}

void
gatecall_status_done(cobj_ref gcs_obj, dj_delivery_code res)
{
    segment_writer<dj_delivery_code> sw(gcs_obj);
    *sw.addr() = res;
    sys_sync_wakeup((uint64_t *) sw.addr());
}

dj_delivery_code
gatecall_status_wait(cobj_ref gcs_obj)
{
    segment_reader<dj_delivery_code> sr(gcs_obj);
    while (*sr.addr() == 0)
	error_check(sys_sync_wait((uint64_t *) sr.addr(), 0, UINT64(~0)));
    return *sr.addr();
}
