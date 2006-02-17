extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/string.h>
#include <inc/fs.h>
#include <inc/syscall.h>
}

static int tcpsink_debug = 1;

class tcpconn {
public:
    tcpconn(const char *hostname, uint16_t port) : fd_(-1) {
	uint32_t ip = 0;
	const char *p = hostname;

	for (int i = 0; i < 4; i++) {
	    if (p == 0)
		panic("bad ip address: %s", hostname);

	    int v = atoi(p);
	    p = strchr(p, '.');
	    if (p != 0)
		p++;

	    if (v < 0 || v >= 256)
		panic("bad ip octet: %s", hostname);

	    ip = (ip << 8) | v;
	}

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(ip);

	fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_ < 0)
	    panic("socket: %s", e2s(fd_));

	if (tcpsink_debug)
	    printf("--- about to connect ---\n");

	int r = connect(fd_, (struct sockaddr *) &sin, sizeof(sin));
	if (r < 0)
	    panic("connect: %s", e2s(r));
    }

    ~tcpconn() {
	if (fd_ >= 0)
	    close(fd_);
    }

    void write(const char *buf, size_t count) {
	size_t done = 0;

	while (done < count) {
	    ssize_t r = ::write(fd_, buf + done, count - done);
	    if (r <= 0)
		panic("cannot write: %s", e2s(r));
	    done += r;
	}
    }

    size_t read(char *buf, size_t len) {
	ssize_t r = ::read(fd_, buf, len);
	if (r < 0)
	    panic("cannot read: %s", e2s(r));

	return r;
    }

private:
    int fd_;
};

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s ipaddr\n", av[0]);
	return -1;
    }

    const char *host = av[1];

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
}
