#ifndef JOS_INC_NETDLINUX_H
#define JOS_INC_NETDLINUX_H

#include <inc/jcomm.h>
#include <inc/netd.h>

struct socket_conn {
    //struct netd_op_socket_args socket_args;
    struct jcomm_ref socket_comm;
    uint64_t container;
    uint64_t taint;
    uint64_t grant;
};

struct op_return {
    int errno;
};

typedef void (*netd_socket_handler)(struct socket_conn *);
int netd_linux_server_init(netd_socket_handler h);

#endif
