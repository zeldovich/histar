#ifndef LINUX_ARCH_LIND_NETDUSER_H
#define LINUX_ARCH_LIND_NETDUSER_H

#include <netd/netdlinux.h>

typedef enum {
    jos64_op_accept,
    jos64_op_recv,
    jos64_op_shutdown,
} jos64_op_t;

struct jos64_op_accept_args {
    struct netd_op_accept_args a;
};

struct jos64_op_recv_args {
    char buf[4000];
    uint64_t cnt;
    uint64_t off;
};

struct jos64_op_shutdown_args {};

struct jos64_op_args {
    jos64_op_t op_type;
    union {
	struct jos64_op_accept_args accept;
	struct jos64_op_recv_args recv;
	struct jos64_op_shutdown_args shutdown;
    };
};

struct sock_slot {
    char used;
    struct socket_conn conn;
    uint64_t linuxpid;
    int sock;
    char listen;
    
    struct netd_op_args opbuf;
    struct jos64_op_args josbuf;
    char outbuf[4096];
    volatile uint64_t opfull;
    volatile uint64_t josfull;
    volatile uint64_t outcnt;
#define CNT_LIMBO UINT64(~0)
};

/* netd_jos64.c */
void jos64_socket_thread(struct socket_conn *sc);

/* netd_slot.c */
struct sock_slot *slot_alloc(void);
struct sock_slot *slot_from_id(int id);
int slot_to_id(struct sock_slot *ss);
void slot_free(struct sock_slot *ss);
void slot_for_each(void (*op)(struct sock_slot*, void*), void *arg);
void slot_init(void);

#endif
