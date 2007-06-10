#ifndef JOS_INC_NETD_H
#define JOS_INC_NETD_H

#include <inc/types.h>
#include <inc/container.h>
#include <sys/select.h>
#include <inc/multisync.h>

struct Fd;
struct netd_ioctl_args;

// We define our own sockaddr_in because we need to translate
// between libc and lwip sockaddr_in equivalents.
struct netd_sockaddr_in {
    // These are network-order (big-endian)
    uint16_t sin_port;
    uint32_t sin_addr;
};

enum { netd_buf_size = 8192 };

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
    netd_op_sendto,
    netd_op_recvfrom,
    netd_op_notify,
    netd_op_probe,
    netd_op_statsync,
    netd_op_shutdown,
    netd_op_ioctl,
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
    struct wait_stat wstat;
};

struct netd_op_shutdown_args {
    int fd;
    int how;
};

struct netd_ioctl_gifconf {
    char name[16];
    struct netd_sockaddr_in addr;
};

struct netd_ioctl_gifflags {
    char name[16];
    int16_t flags;
};

struct netd_ioctl_gifbrdaddr {
    char name[16];
    struct netd_sockaddr_in baddr;
};

struct netd_op_ioctl_args {
    uint64_t libc_ioctl;
    union {
	struct netd_ioctl_gifconf gifconf;
	struct netd_ioctl_gifflags gifflags;
	struct netd_ioctl_gifbrdaddr gifbrdaddr;
    };
};

struct netd_op_args {
    netd_op_t op_type;
    int size;
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

struct netd_ipc_segment {
    uint64_t sync;
    struct netd_op_args args;
};

#define NETD_IPC_SYNC_REPLY	0x00
#define NETD_IPC_SYNC_REQUEST	0x01

#define NETD_SEL_SYNC_DONE      0x00
#define NETD_SEL_SYNC_REQUEST   0x01
#define NETD_SEL_SYNC_CLOSE     0x02


// match dev_probe_t
typedef enum {
    netd_sel_op_read = 0,
    netd_sel_op_write,
    netd_sel_op_count,
} netd_sel_op_t;

typedef void (*netd_handler)(struct netd_op_args *);

typedef enum {
    netd_if_jif,
    netd_if_tun,
} netd_dev_type;

void netd_lwip_init(void (*cb)(void*), void *cbarg,
		    netd_dev_type type, void *if_state,
		    uint32_t ipaddr, uint32_t netmask, uint32_t gw)
    __attribute__((noreturn));

int netd_lwip_ioctl(struct netd_op_ioctl_args *a);
int netd_lwip_statsync(struct Fd *fd, struct netd_op_statsync_args *a);
int netd_lwip_probe(struct Fd *fd, struct netd_op_probe_args *a);

int  netd_call(struct Fd *fd, struct netd_op_args *a);
int  netd_slow_call(struct cobj_ref netd_gate, struct netd_op_args *a);
struct cobj_ref netd_get_gate(void);
void netd_set_gate(struct cobj_ref g);

int netd_socket(int domain, int type, int protocol);

struct host_entry {
    const char *alias;
    const char *name;
};

extern const struct host_entry host_table[];

#endif
