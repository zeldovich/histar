#ifndef JOS_INC_LABELUTIL_HH
#define JOS_INC_LABELUTIL_HH

extern "C" {
#include <machine/types.h>
}

#include <inc/cpplabel.hh>

void thread_drop_star(uint64_t handle);
void thread_cur_label(label *l);
void thread_cur_clearance(label *l);

#endif
