#ifndef JOS_INC_NETDLINUX_H
#define JOS_INC_NETDLINUX_H

#include <inc/jcomm.h>
#include <inc/netd.h>

struct socket_conn {
    struct jcomm_ref socket_comm;
    uint64_t container;
    uint64_t taint;
    uint64_t grant;
};

/* XXX drop me */
struct netd_linux_ret {
    int rerrno;
};

typedef void (*netd_socket_handler)(struct socket_conn *);

/* server */
int netd_linux_server_init(netd_socket_handler h);

/* client */
int netd_linux_call(struct Fd *fd, struct netd_op_args *a);

#endif
