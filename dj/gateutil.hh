#ifndef JOS_DJ_GATEUTIL_HH
#define JOS_DJ_GATEUTIL_HH

void dj_gate_call_incoming(const cobj_ref &seg, const label &vl, const label &vc,
			   uint64_t call_grant, uint64_t call_taint,
			   label *argtaint, label *arggrant, str *data);

void dj_gate_call_outgoing(uint64_t ct, uint64_t call_grant, uint64_t call_taint,
			   const label &taint, const label &grant, const str &data,
			   cobj_ref *segp, label *vl, label *vc);

#endif
