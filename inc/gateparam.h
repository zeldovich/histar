#ifndef JOS_INC_GATEPARAM_H
#define JOS_INC_GATEPARAM_H

#include <inc/types.h>
#include <inc/container.h>

struct gate_call_data {
    struct cobj_ref return_gate;

    uint64_t taint_container;
    uint64_t thread_ref_ct;

    uint64_t call_taint;
    uint64_t call_grant;
    struct cobj_ref declassify_gate;

    /*
     * WARNING: this gate_call_data is usually in thread-local memory,
     * and as a result, should only be modified by the correct thread!
     */
    union {
	struct cobj_ref param_obj;
	char param_buf[128];
    };
};

#define gate_call_data_copy(A, B)                  \
    do {                                           \
        (A)->return_gate = (B)->return_gate;		\
        (A)->taint_container = (B)->taint_container;	\
        (A)->declassify_gate = (B)->declassify_gate;	\
    } while(0)

#define gate_call_data_copy_all(A, B)                  \
	memcpy(A, B, sizeof(struct gate_call_data))

#endif
