#ifndef JOS_DJ_GATECALLSTATUS_H
#define JOS_DJ_GATECALLSTATUS_H

class label;

cobj_ref gatecall_status_alloc(uint64_t ct, label *l);
void     gatecall_status_done(cobj_ref gcs_obj, uint64_t res);
uint64_t gatecall_status_wait(cobj_ref gcs_obj);

#endif
