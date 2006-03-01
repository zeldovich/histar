#ifndef JOS_INC_GATEPARAM_HH
#define JOS_INC_GATEPARAM_HH

struct gate_call_data {
    struct cobj_ref return_gate;

    // WARNING: this gate_call_data is usually in thread-local memory,
    // and as a result, should only be modified by the correct thread!
    union {
	struct cobj_ref param_obj;
	char param_buf[32];
    };
};

#endif
