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

    uint64_t call_handle() { return call_handle_; }
    label *obj_label() { return obj_label_; }
    uint64_t taint_ct() { return taint_ct_obj_.object; }

    void call(gate_call_data *gcd,
	      label *verify);			// { 3 } for none

private:
    int64_t call_handle_;
    cobj_ref gate_;
    cobj_ref taint_ct_obj_;

    label *tgt_label_;
    label *tgt_clear_;
    label *obj_label_;
};

#endif
