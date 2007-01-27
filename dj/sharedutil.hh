#ifndef JOS_INC_DIS_SHAREDUTIL_HH
#define JOS_INC_DIS_SHAREDUTIL_HH

int64_t gate_send(struct cobj_ref gate, void *args, int n, label *ds);
void  shared_grant_cat(uint64_t shared_id, uint64_t cat);

#endif
