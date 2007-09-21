#ifndef JOS_DJ_GATECALLSTATUS_HH
#define JOS_DJ_GATECALLSTATUS_HH

#include <dj/djprotx.h>

class label;

cobj_ref gatecall_status_alloc(uint64_t ct, label *l);
void gatecall_status_done(cobj_ref gcs_obj, dj_delivery_code res);
dj_delivery_code gatecall_status_wait(cobj_ref gcs_obj);

#endif
