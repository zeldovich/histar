#ifndef JOS_INC_NETD_H
#define JOS_INC_NETD_H

#include <inc/types.h>
#include <inc/container.h>
#include <sys/select.h>

// We define our own sockaddr_in because we need to translate
// between libc and lwip sockaddr_in equivalents.
struct netd_sockaddr_in {
    // These are network-order (big-endian)
    uint16_t sin_port;
    uint32_t sin_addr;
};

enum { netd_buf_size = 3500 };

typedef enum {
    netd_op_socket,
    netd_op_bind,
    netd_op_listen,
    netd_op_accept,
    netd_op_connect,
    netd_op_close,
    netd_op_getsockname,
    netd_op_getpeername,
    netd_op_setsockopt,
    netd_op_getsockopt,
    netd_op_send,
    netd_op_recv,
    netd_op_select,
    netd_op_shutdown,
} netd_op_t;

struct netd_op_socket_args {
    int domain;
    int type;
    int protocol;
};

struct netd_op_bind_args {
    int fd;		
    struct netd_sockaddr_in sin;
};

struct netd_op_listen_args {
    int fd;
    int backlog;
};

struct netd_op_accept_args {
    int fd;
    struct netd_sockaddr_in sin;
};

struct netd_op_connect_args {
    int fd;
    struct netd_sockaddr_in sin;
};

struct netd_op_send_args {
    int fd;
    uint32_t count;
    int flags;
    char buf[netd_buf_size];
};

struct netd_op_recv_args {
    int fd;
    uint32_t count;
    int flags;
    char buf[netd_buf_size];
};

struct netd_op_close_args {
    int fd;
};

struct netd_op_getsockname_args {
    int fd;
    struct netd_sockaddr_in sin;
};

struct netd_op_getpeername_args {
    int fd;     
    struct netd_sockaddr_in sin;
};

struct netd_op_setsockopt_args {
    int fd;
    int level;
    int optname;
    char optval[16];
    uint32_t optlen;
};

struct netd_op_getsockopt_args {
    int fd;
    int level;
    int optname;
    char optval[16];
    uint32_t optlen;
};

struct netd_op_select_args {
    int fd;
    char write;
};

struct netd_op_shutdown_args {
    int fd;
    int how;
};

struct netd_op_args {
    netd_op_t op_type;
    int rval;

    union {
	struct netd_op_socket_args socket;
	struct netd_op_bind_args bind;
	struct netd_op_listen_args listen;
	struct netd_op_accept_args accept;
	struct netd_op_connect_args connect;
	struct netd_op_close_args close;
	struct netd_op_getsockname_args getsockname;
	struct netd_op_getpeername_args getpeername;
	struct netd_op_setsockopt_args setsockopt;
	struct netd_op_getsockopt_args getsockopt;
	struct netd_op_recv_args recv;
	struct netd_op_send_args send;
	struct netd_op_select_args select;
	struct netd_op_shutdown_args shutdown;
    };
};

typedef enum {
    netd_if_jif,
    netd_if_tun,
} netd_dev_type;

void netd_lwip_init(void (*cb)(void*), void *cbarg,
		    netd_dev_type type, void *if_state,
		    uint32_t ipaddr, uint32_t netmask, uint32_t gw)
    __attribute__((noreturn));

void netd_dispatch(struct netd_op_args *a);

int  netd_call(struct cobj_ref netd_gate, struct netd_op_args *a);
struct cobj_ref netd_get_gate(void);
void netd_set_gate(struct cobj_ref g);

struct host_entry {
    const char *alias;
    const char *name;
};

extern const struct host_entry host_table[];

#endif
