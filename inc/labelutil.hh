#ifndef JOS_INC_LABELUTIL_HH
#define JOS_INC_LABELUTIL_HH

extern "C" {
#include <inc/types.h>
#include <inc/labelutil.h>
}

#include <inc/cpplabel.hh>

void thread_drop_star(uint64_t handle);
void thread_drop_starpair(uint64_t h1, uint64_t h2);

int  thread_set_label(label *l);
int  thread_set_clearance(label *l);

void thread_cur_label(label *l);
void thread_cur_clearance(label *l);

void thread_cur_verify(label *l);

void obj_get_label(struct cobj_ref o, label *l);
void gate_get_clearance(struct cobj_ref o, label *l);

void get_label_retry(label *l, int (*fn) (struct ulabel *));
#endif
