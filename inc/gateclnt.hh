#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH

#include <inc/cpplabel.hh>

void gate_call(struct cobj_ref gate, struct gate_call_data *gcd,
	       label *contaminate_label,		// { * } for none
	       label *decontaminate_label,		// { 3 } for none
	       label *decontaminate_clearance);		// { 0 } for none

#endif
