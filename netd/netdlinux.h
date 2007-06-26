#ifndef JOS_INC_NETDLINUX_H
#define JOS_INC_NETDLINUX_H

#include <inc/jcomm.h>
#include <inc/netd.h>

#define NETD_LINUX_MAGIC UINT64(0x31960B4)

struct socket_conn {
    uint64_t init_magic;
    int sock_id;

    struct jcomm_ref ctrl_comm;
    struct jcomm_ref data_comm;
    uint64_t container;
    uint64_t taint;
    uint64_t grant;
};

typedef void (*netd_socket_handler)(struct socket_conn *);

/* server */
int netd_linux_server_init(netd_socket_handler h, uint64_t inet_taint);

/* client */
int netd_linux_call(struct Fd *fd, struct netd_op_args *a);
int netd_linux_client_init(struct cobj_ref *gate);

#endif
