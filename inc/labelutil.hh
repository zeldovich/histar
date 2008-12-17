#ifndef JOS_INC_LABELUTIL_HH
#define JOS_INC_LABELUTIL_HH

extern "C" {
#include <inc/types.h>
#include <inc/labelutil.h>
}

#include <inc/cpplabel.hh>

void thread_drop_star(uint64_t cat);
void thread_drop_starpair(uint64_t c1, uint64_t c2);

int  thread_set_label(label *l);
int  thread_set_ownership(label *l);
int  thread_set_clearance(label *l);

void thread_cur_label(label *l);
void thread_cur_ownership(label *l);
void thread_cur_clearance(label *l);
void thread_cur_verify(label *o, label *c);
void thread_label_cache_update(label *l, label *o, label *c);

void obj_get_label(struct cobj_ref o, label *l);
void obj_get_ownership(struct cobj_ref o, label *l);
void obj_get_clearance(struct cobj_ref o, label *l);

void get_label_retry(label *l, int (*fn) (struct ulabel *));

#endif
