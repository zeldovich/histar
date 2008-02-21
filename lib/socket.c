#include <inc/sockfd.h>
#include <inc/uds.h>
#include <inc/bipipe.h>
#include <inc/stdio.h>
#include <inc/debug.h>

#include <sys/socket.h>
#include <errno.h>

libc_hidden_proto(socket)

int
socket(int domain, int type, int protocol)
{
    jos_trace("%d, %d, %d", domain, type, protocol);
    switch(domain) {
    case PF_INET:
	return netd_socket(domain, type, protocol);

    case PF_UNIX:
	return uds_socket(domain, type, protocol);

    default:
	__set_errno(EAFNOSUPPORT);
	return -1;
    }
}

int
socketpair(int domain, int type, int protocol, int sv[2])
{
    jos_trace("%d, %d, %d, %p", domain, type, protocol, sv);
    // fudge the socketpair
    return bipipe(type, sv);
}

libc_hidden_def(socket)

