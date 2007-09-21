#ifndef JOS_INC_NETD_OPNUM_H
#define JOS_INC_NETD_OPNUM_H

#define ALL_NETD_OPS		\
    NETD_OP_ENTRY(socket)	\
    NETD_OP_ENTRY(bind)		\
    NETD_OP_ENTRY(listen)	\
    NETD_OP_ENTRY(accept)	\
    NETD_OP_ENTRY(connect)	\
    NETD_OP_ENTRY(close)	\
    NETD_OP_ENTRY(getsockname)	\
    NETD_OP_ENTRY(getpeername)	\
    NETD_OP_ENTRY(setsockopt)	\
    NETD_OP_ENTRY(getsockopt)	\
    NETD_OP_ENTRY(send)		\
    NETD_OP_ENTRY(sendto)	\
    NETD_OP_ENTRY(recvfrom)	\
    NETD_OP_ENTRY(notify)	\
    NETD_OP_ENTRY(probe)	\
    NETD_OP_ENTRY(statsync)	\
    NETD_OP_ENTRY(shutdown)	\
    NETD_OP_ENTRY(ioctl)	\

#define NETD_OP_ENTRY(name)	netd_op_##name,

typedef enum {
    ALL_NETD_OPS
    netd_op_max
} netd_op_t;

#undef NETD_OP_ENTRY

#endif
