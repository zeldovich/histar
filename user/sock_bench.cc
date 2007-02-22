#ifndef JOS_TEST
#define JOS64 1
#define LINUX 0
#else
#define JOS64 0
#define LINUX 1
#endif

#ifdef LWIP_LIB
#define LWIP 1
#else
#define LWIP 0
#endif

extern "C" {
#if JOS64
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/fd.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#if LWIP
#include <lif/socket.h>
#include <lwip/inet.h>
#include <lif/init.h>
#include <arch/sys_arch.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#if LINUX
#include <pthread.h>
#endif
}

#include <inc/errno.hh>

static char threaded = 1;
static int byte_count = 150;
static int iter_count = 5;

// for lif/socket.h macros
static void
nb_read(int s, char *b, int count)
{
    while (count) {
	int r = read(s, b, count);
	if (r < 0)
	    throw basic_exception("read: %s", strerror(errno));
	if (r == 0)
	    throw basic_exception("read: EOF");
	count -= r;
	b += r;
    }
}

static void
nb_write(int s, char *b, int count)
{
    while (count) {
	int r = write(s, b, count);
	if (r < 0)
	    throw basic_exception("write: %s", strerror(errno));
	if (r == 0)
	    throw basic_exception("write: unable to write");
	count -= r;
	b += r;
    }
}

#if JOS64
static void
#else
static void*
#endif
server(void *arg)
{
    static char buf[4096];
    int s = (int)(int64_t)arg;
    
    if (byte_count) {
	try {
	    for (int i = 0; i < iter_count; i++) {
		nb_write(s, buf, byte_count);
		nb_read(s, buf, byte_count);
	    }
	} catch (std::exception &e) {
	    printf("server error: %s\n", e.what());
	}
    }
    close(s);
}

static void
start_server_thread(int s)
{
    int r;
#if JOS64
    struct cobj_ref t;
    r = thread_create(start_env->proc_container, &server,
		      (void*) (int64_t) s, &t, "server");
    if (r < 0) {
	printf("cannot spawn client thread: %s\n", e2s(r));
	close(s);
    } else {
	fd_give_up_privilege(s);
    }
#else 
    pthread_t t;
    r = pthread_create(&t, 0, server, (void *)(int64_t)s);
#endif
}

int
main (int ac, char **av)
{
    uint16_t port = 9999;

#if LWIP
    // VMWare MAC
    const char mac_addr[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x10 };
    char *iface = av[1];
    printf("sock_bench: lwip_init, iface %s, MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
	   iface, mac_addr[0] & 0xFF, mac_addr[1] & 0xFF, mac_addr[2] & 0xFF, 
	   mac_addr[3] & 0xFF, mac_addr[4] & 0xFF, mac_addr[5] & 0xFF);
    error_check(lwip_init(iface, mac_addr));
#endif

    int c;
    while ((c = getopt(ac, av, "p:b:i:t:")) != -1) {
	switch(c) {
	case 'b':
	    byte_count = atoi(optarg);
	    break;
	case 'i':
	    iter_count = atoi(optarg);
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 't':
	    threaded = atoi(optarg);
	    break;
	default:
	    printf("unreconized option: %c\n", c);
	}
    }
    
    int s;
    errno_check(s = socket(AF_INET, SOCK_STREAM, 0));
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    errno_check(bind(s, (struct sockaddr *)&sin, sizeof(sin)));

    errno_check(listen(s, 5));
    
    printf("sock_bench: byte count %d, iter count %d, port %d\n", 
	   byte_count, iter_count, port);
    for (;;) {
        socklen_t socklen = sizeof(sin);
	
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
	    printf("cannot accept client: %d\n", ss);
            continue;
        }
	
	if (threaded)
	    start_server_thread(ss);
	else
	    server((void *) (int64_t)ss);	
    }
    return 0;
}
