#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>

// The only reason that gate_call is an object is that we want to
// delay the garbage-collection of the gate call container until
// we had a chance to copy out any segments that were in there.
class gate_call {
public:
    gate_call(struct cobj_ref gate, struct gate_call_data *gcd,
	      label *contaminate_label,		// { * } for none
	      label *decontaminate_label,	// { 3 } for none
	      label *decontaminate_clearance);	// { 0 } for none
    ~gate_call();

private:
    struct cobj_ref taint_ct_obj_;
};

#endif
