#ifndef JOS_NETD_NETDIPC_H
#define JOS_NETD_NETDIPC_H

struct netd_ipc_segment {
    uint64_t sync;
    struct netd_op_args args;
};

#define NETD_IPC_SYNC_REPLY	0x00
#define NETD_IPC_SYNC_REQUEST	0x01

#define NETD_SEL_SYNC_DONE      0x00
#define NETD_SEL_SYNC_REQUEST   0x01
#define NETD_SEL_SYNC_CLOSE     0x02

#endif
