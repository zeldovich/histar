#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <inc/netd.h>

static void
telnet_server(void)
{
    int s = netd_socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
        panic("cannot create socket: %d\n", s);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(23);
    int r = netd_bind(s, (struct sockaddr *)&sin, sizeof(sin));
    if (r < 0)
        panic("cannot bind socket: %d\n", r);

    r = netd_listen(s, 5);
    if (r < 0)
        panic("cannot listen on socket: %d\n", r);

    cprintf("netd: server on port 23\n");
    for (;;) {
        socklen_t socklen = sizeof(sin);
        int ss = netd_accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            cprintf("cannot accept client: %d\n", ss);
            continue;
        }

        char *msg = "Hello world.\n";
        netd_write(ss, msg, strlen(msg));
        netd_close(ss);
    }
}

int
main(int ac, char **av)
{
    uint64_t myct = start_arg;

    int r = netd_client_init(myct);
    if (r < 0)
	panic("initializing netd client: %s", e2s(r));

    telnet_server();
}
