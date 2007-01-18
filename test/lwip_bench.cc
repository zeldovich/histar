extern "C" {
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lif/init.h>

#include <pthread.h>
}


static char buf[4096];
static uint64_t byte_count = 0;

static const char threaded = 1;

#define err_exit(__exp, __frmt, __args...)				\
    do {								\
	if (__exp) {							\
	    printf("error %s: " __frmt "\n", __FUNCTION__, ##__args);	\
	    exit(1);							\
	}								\
    } while (0)

static void*
server(void *arg)
{
    int s = (int)(int64_t)arg;

    if (byte_count) {
	for (int i = 0; i < 5; i++) {
	    int r = lwip_write(s, buf, byte_count);
	    if (r < 0 || (uint32_t) r < byte_count)
		err_exit(1, "write error: %s\n", strerror(errno));
	    
	    r = lwip_read(s, buf, byte_count);
	    if (r < 0 || (uint32_t) r < byte_count)
		err_exit(1, "read error: %s\n", strerror(errno));
	}
    }
    close(s);
}

void
server_cb(void *a)
{
    ;
}

// VMWare MAC
static char mac_addr[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x10 };

int
main (int ac, char **av)
{
    if (ac < 2) {
	printf("usage: %s iface [-b byte-count]\n", av[0]);
	exit(1);
    }

    char *iface = av[1];
    int r = lwip_init(server_cb, 0, iface, mac_addr);
    err_exit(r < 0, "lwip_init failed");
    
    uint16_t port = 9999;

    int c;
    while ((c = getopt(ac, av, "b:")) != -1) {
	switch(c) {
	case 'b':
	    byte_count = atoi(optarg);
	    break;
	}
    }
    
    int s = lwip_socket(AF_INET, SOCK_STREAM, 0);
    err_exit(s < 0, "cannot create socket: %s", strerror(errno));
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    r = lwip_bind(s, (struct sockaddr *)&sin, sizeof(sin));
    err_exit(r < 0, "cannot bind socket: %s", strerror(errno));
    
    r = lwip_listen(s, 5);
    err_exit(r < 0, "cannot listen on socket: %s", strerror(r));
    
    printf("sock_bench: server on port %d\n", port);
    for (;;) {
        socklen_t socklen = sizeof(sin);
	
        int ss = lwip_accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
	    printf("cannot accept client: %d\n", ss);
            continue;
        }
	
	if (threaded) {
	    pthread_t t;
	    r = pthread_create(&t, 0, server, (void *)(int64_t)ss);
	} else
	    server((void *) (int64_t)ss);	
    }
    return 0;
}
