#ifndef JOS_DJ_GATEUTIL_HH
#define JOS_DJ_GATEUTIL_HH

extern "C" {
#include <inc/gateparam.h>
}

void dj_gate_call_incoming(const cobj_ref &seg, const label &vl, const label &vc,
			   uint64_t call_grant, uint64_t call_taint,
			   label *argtaint, label *arggrant, str *data);

void dj_gate_call_outgoing(uint64_t ct, uint64_t call_grant, uint64_t call_taint,
			   const label &taint, const label &grant, const str &data,
			   cobj_ref *segp, label *vl, label *vc);

void dj_gate_call_save_tainted(uint64_t ct, gate_call_data *gcd,
			       cobj_ref *segp);

void dj_gate_call_load_taint(cobj_ref taintseg, gate_call_data *gcd,
			     label *vl, label *vc);

struct dj_gate_call_tainted {
    cobj_ref seg;
    cobj_ref rgate;
    uint64_t taint_ct;
    uint64_t call_grant;
    uint64_t call_taint;
    ulabel vl;
    ulabel vc;
    uint64_t labelent[0];
};

#endif
