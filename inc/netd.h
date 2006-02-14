#ifndef JOS_INC_NETD_H
#define JOS_INC_NETD_H

#include <lwip/inet.h>
#include <lwip/sockets.h>

typedef enum {
    netd_op_socket,
    netd_op_bind,
    netd_op_listen,
    netd_op_accept,
    netd_op_connect,
    netd_op_write,
    netd_op_read,
    netd_op_close
} netd_op_t;

struct netd_op_socket_args {
    int domain;
    int type;
    int protocol;
};

struct netd_op_bind_args {
    int fd;		
    struct sockaddr_in sin;
};

struct netd_op_listen_args {
    int fd;
    int backlog;
};

struct netd_op_accept_args {
    int fd;
    struct sockaddr_in sin;
};

struct netd_op_connect_args {
    int fd;
    struct sockaddr_in sin;
};

struct netd_op_write_args {
    int fd;
    uint32_t count;
    char buf[1024];
};

struct netd_op_read_args {
    int fd;
    uint32_t count;
    char buf[1024];
};

struct netd_op_close_args {
    int fd;
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
	struct netd_op_write_args write;
	struct netd_op_read_args read;
	struct netd_op_close_args close;
    };
};

int netd_server_init(uint64_t container);
int socket(int domain, int type, int protocol);

#endif
