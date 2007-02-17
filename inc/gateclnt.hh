#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>

class gate_call {
 public:
    gate_call(cobj_ref gate,
	      const label *contaminate_label,		// { 0 } for none
	      const label *decontaminate_label,		// { 3 } for none
	      const label *decontaminate_clearance);	// { 0 } for none
    ~gate_call();

    uint64_t call_ct() { return call_ct_obj_.object; }
    uint64_t call_taint() { return call_taint_; }
    uint64_t call_grant() { return call_grant_; }
    cobj_ref return_gate() { return return_gate_; }

    void call(gate_call_data *gcd,
	      const label *verify_label = 0,		// { 3 } for none
	      const label *verify_clear = 0,		// { 0 } for none
	      void (*return_cb)(void*) = 0, void *cbarg = 0);

 private:
    gate_call(const gate_call&);
    gate_call &operator=(const gate_call&);

    void set_verify(const label *vl, const label *vc);

    int64_t call_taint_, call_grant_;
    cobj_ref gate_;
    cobj_ref call_ct_obj_;
    cobj_ref taint_ct_obj_;
    cobj_ref return_gate_;

    label *tgt_label_;
    label *tgt_clear_;
};

#endif
