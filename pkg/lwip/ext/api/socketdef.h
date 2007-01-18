#include <lwip/sys.h>

#define NUM_SOCKETS MEMP_NUM_NETCONN

struct lwip_socket {
    struct netconn *conn;
    struct netbuf *lastdata;
    u16_t lastoffset;
    uint64_t rcvevent;
    uint64_t sendevent;
    u16_t  flags;
    int err;
};
