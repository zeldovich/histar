#ifndef JOS_INC_GATECLNT_HH
#define JOS_INC_GATECLNT_HH



void gate_call(struct cobj_ref gate, struct cobj_ref *param,
	       struct ulabel *label, struct ulabel *clearance);

#endif
