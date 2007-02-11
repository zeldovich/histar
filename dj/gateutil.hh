#ifndef JOS_DJ_GATEUTIL_HH
#define JOS_DJ_GATEUTIL_HH

void dj_gate_call_incoming(cobj_ref seg, const label &vl, const label &vc,
			   uint64_t call_grant, uint64_t call_taint,
			   label *argtaint, label *arggrant, str *data);

#endif
