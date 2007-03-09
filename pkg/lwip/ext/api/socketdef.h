#include <lwip/sys.h>

#define NUM_SOCKETS MEMP_NUM_NETCONN

struct lwip_socket {
    struct netconn *conn;
    struct netbuf *lastdata;
    uint64_t rcvevent;
    uint64_t sendevent;
    u16_t lastoffset;
    u16_t flags;
    int err;

    int recv_wakeup;
    int send_wakeup;
};
