#ifndef JOS_INC_GATEINVOKE_HH
#define JOS_INC_GATEINVOKE_HH

#include <inc/cpplabel.hh>

typedef void (*gate_invoke_cb) (label *, label *, label *, void *);

void gate_invoke(struct cobj_ref gate, label *cs, label *ds, label *dr,
		 gate_invoke_cb cb, void *arg)
    __attribute__((noreturn));

#endif
