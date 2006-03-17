#ifndef JOS_INC_GATEPARAM_H
#define JOS_INC_GATEPARAM_H

#include <inc/types.h>
#include <inc/container.h>

struct gate_call_data {
    struct cobj_ref return_gate;
    uint64_t taint_container;

    // WARNING: this gate_call_data is usually in thread-local memory,
    // and as a result, should only be modified by the correct thread!
    union {
	struct cobj_ref param_obj;
	char param_buf[128];
    };
};

#endif
