#ifndef JOS_INC_GATEINVOKE_HH
#define JOS_INC_GATEINVOKE_HH

#include <inc/cpplabel.hh>

void gate_invoke(struct cobj_ref gate, label *cs, label *ds, label *dr)
    __attribute__((noreturn));

#endif
