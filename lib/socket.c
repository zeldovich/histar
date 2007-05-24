#include <inc/netd.h>
#include <inc/bipipe.h>

#include <sys/socket.h>

int
socket(int domain, int type, int protocol)
{
    return netd_socket(domain, type, protocol);
}

int
socketpair(int domain, int type, int protocol, int sv[2])
{
    // fudge the socketpair
    return bipipe(sv);
}
