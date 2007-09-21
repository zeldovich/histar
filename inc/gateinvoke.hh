#ifndef JOS_INC_GATEINVOKE_HH
#define JOS_INC_GATEINVOKE_HH

#include <inc/cpplabel.hh>

typedef void (*gate_invoke_cb) (label *tgt_s, label *tgt_r,
				void *arg);

void gate_compute_labels(struct cobj_ref gate,
			 const label *cs, const label *ds, const label *dr,
			 label *tgt_s, label *tgt_r);

void gate_invoke(struct cobj_ref gate, label *tgt_s, label *tgt_r,
		 gate_invoke_cb cb, void *arg)
    __attribute__((noreturn));

#endif
