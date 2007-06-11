#ifndef JOS_INC_NETDCLNT_HH
#define JOS_INC_NETDCLNT_HH

#include <inc/gateclnt.hh>

extern "C" {
#include <inc/netd.h>
}

struct netd_fast_ipc_state {
    struct netd_ipc_segment *fast_ipc;
    gate_call *fast_ipc_gatecall;
    cobj_ref fast_ipc_gate;
    uint64_t fast_ipc_inited;
    uint64_t fast_ipc_inited_shared_ct;
    jthread_mutex_t fast_ipc_mu;
};

void netd_fast_call(struct netd_op_args *a);
int  netd_slow_call(struct cobj_ref gate, struct netd_op_args *a);

#endif
