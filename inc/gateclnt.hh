#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>

class gate_call {
public:
    gate_call(cobj_ref gate,
	      label *contaminate_label,		// { * } for none
	      label *decontaminate_label,	// { 3 } for none
	      label *decontaminate_clearance);	// { 0 } for none
    ~gate_call();

    uint64_t call_ct() { return call_ct_obj_.object; }

    void call(gate_call_data *gcd,
	      label *verify);			// { 3 } for none

private:
    void set_verify(label *verify);

    int64_t call_taint_, call_grant_;
    cobj_ref gate_;
    cobj_ref call_ct_obj_;
    cobj_ref taint_ct_obj_;
    cobj_ref return_gate_;

    label *tgt_label_;
    label *tgt_clear_;
    label *obj_label_;
};

#endif
