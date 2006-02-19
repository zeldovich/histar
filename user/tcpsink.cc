extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/string.h>
#include <inc/fs.h>
#include <inc/syscall.h>
}
#include <inc/nethelper.hh>

static int tcpsink_debug = 1;

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s ipaddr\n", av[0]);
	return -1;
    }

    const char *host = av[1];

    try {
	tcpconn tc(host, 19);
	if (tcpsink_debug)
	    printf("Connected OK\n");

	char buf[1024];
	for (;;) {
	    size_t r = tc.read(&buf[0], sizeof(buf));
	    printf("got %ld bytes\n", r);
	    if (r <= 0)
		break;
	}
    } catch (std::exception &e) {
	printf("error: %s\n", e.what());
    }
}
