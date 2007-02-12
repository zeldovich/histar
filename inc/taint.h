#ifndef JOS_INC_TAINT_H
#define JOS_INC_TAINT_H

int  taint_cow(uint64_t taint_container, struct cobj_ref declassify_gate);
void taint_set_checkpoint(struct cobj_ref ckpt_as);

#endif
