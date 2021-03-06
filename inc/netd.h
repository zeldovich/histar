#ifndef JOS_INC_NETD_H
#define JOS_INC_NETD_H

#include <inc/types.h>
#include <inc/container.h>
#include <inc/multisync.h>
#include <inc/netd_opnum.h>

#include <sys/select.h>

struct Fd;

// We define our own sockaddr_in because we need to translate
// between libc and lwip sockaddr_in equivalents.
struct netd_sockaddr_in {
    // These are network-order (big-endian)
    uint16_t sin_port;
    uint32_t sin_addr;
};

enum { netd_buf_size = 8192 };

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

struct netd_op_sendto_args {
    int fd;
    uint32_t count;
    int flags;
    struct netd_sockaddr_in sin;
    char buf[netd_buf_size];
};

struct netd_op_recvfrom_args {
    int fd;
    uint32_t wantfrom : 1;
    uint32_t count : 31;
    int flags;

    /* "sin" must be immediately followed by "buf" */
    struct netd_sockaddr_in sin;
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
    char optval[64];
    uint32_t optlen;
};

struct netd_op_getsockopt_args {
    int fd;
    int level;
    int optname;
    char optval[64];
    uint32_t optlen;
};

struct netd_op_notify_args {
    int fd;
    dev_probe_t how;
};

struct netd_op_probe_args {
    int fd;
    dev_probe_t how;
};

struct netd_op_statsync_args {
    int fd;
    dev_probe_t how;
    struct wait_stat wstat[2];
};

struct netd_op_shutdown_args {
    int fd;
    int how;
};

struct netd_ioctl_gifconf {
    struct {
	char name[16];
	struct netd_sockaddr_in addr;
    } ifs[16];
    uint32_t ifcount;
};

struct netd_ioctl_gifflags {
    char name[16];
    int16_t flags;
};

struct netd_ioctl_gifaddr {
    char name[16];
    struct netd_sockaddr_in addr;
};

struct netd_ioctl_gifhwaddr {
    char name[16];
    int hwfamily;
    int hwlen;
    char hwaddr[16];
};

struct netd_ioctl_gifint {
    char name[16];
    int val;
};

struct netd_op_ioctl_args {
    uint64_t libc_ioctl;
    union {
	struct netd_ioctl_gifconf gifconf;
	struct netd_ioctl_gifflags gifflags;
	struct netd_ioctl_gifaddr gifaddr;
	struct netd_ioctl_gifhwaddr gifhwaddr;
	struct netd_ioctl_gifint gifint;
	int intval;
    };
};

struct netd_op_args {
    uint32_t op_type;
    uint32_t size;
    int rval;
    int rerrno;

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
	struct netd_op_recvfrom_args recvfrom;
	struct netd_op_send_args send;
	struct netd_op_sendto_args sendto;
	struct netd_op_notify_args notify;
	struct netd_op_probe_args probe;
	struct netd_op_statsync_args statsync;
	struct netd_op_shutdown_args shutdown;
	struct netd_op_ioctl_args ioctl;
    };
};

int  netd_call(struct Fd *fd, struct netd_op_args *a);
struct cobj_ref netd_get_gate(void);

#endif
