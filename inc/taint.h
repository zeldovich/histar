#ifndef JOS_INC_TAINT_H
#define JOS_INC_TAINT_H

int taint_cow(uint64_t taint_container, struct cobj_ref declassify_gate);

#endif
