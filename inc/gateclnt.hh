#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH

extern "C" {
#include <inc/container.h>
}

#include <inc/cpplabel.hh>

class gate_call {
 public:
    gate_call(cobj_ref gate, const label *owner, const label *clear);
    ~gate_call();

    uint64_t call_ct() { return call_ct_obj_.object; }
    uint64_t call_taint() { return call_taint_; }
    uint64_t call_grant() { return call_grant_; }
    cobj_ref return_gate() { return return_gate_; }

    void call(gate_call_data *gcd,
	      const label *verify_owner = 0,
	      const label *verify_clear = 0,
	      void (*return_cb)(void*) = 0, void *cbarg = 0,
	      bool setup_return_gate = true);
 private:
    gate_call(const gate_call&);
    gate_call &operator=(const gate_call&);

    void set_verify(const label *vl, const label *vc);

    int64_t call_taint_, call_grant_;
    cobj_ref gate_;
    cobj_ref call_ct_obj_;
    cobj_ref return_gate_;

    label *owner_;
    label *clear_;
};

#endif
