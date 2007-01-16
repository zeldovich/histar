#ifdef JOS_USER
#define JOS64 1
#define LINUX 0
#else
#define JOS64 0
#define LINUX 1
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

#include <sys/socket.h>
#include <netinet/in.h>

#if LINUX
#include <pthread.h>
#endif
}

#if JOS64
#include <inc/error.hh>
#endif

static char buf[4096];
static uint64_t byte_count = 0;

static const char threaded = 0;

#define err_exit(__exp, __frmt, __args...)				\
    do {								\
	if (__exp) {							\
	    printf("error %s: " __frmt "\n", __FUNCTION__, ##__args);	\
	    exit(1);							\
	}								\
    } while (0)

#if JOS64
static void
#else
static void*
#endif
server(void *arg)
{
    int s = (int)(int64_t)arg;
    if (byte_count) {
	int r = write(s, buf, byte_count);
	if (r < 0 || (uint32_t) r < byte_count)
	    err_exit(1, "write error: %s\n", strerror(errno));
    }
    close(s);
}

int
main (int ac, char **av)
{
    uint16_t port = 9999;

    int c;
    while ((c = getopt(ac, av, "b:")) != -1) {
	switch(c) {
	case 'b':
	    byte_count = atoi(optarg);
	    break;
	}
    }
    
    int s = socket(AF_INET, SOCK_STREAM, 0);
    err_exit(s < 0, "cannot create socket: %s", strerror(errno));
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    int r = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    err_exit(r < 0, "cannot bind socket: %s", strerror(errno));
    
    r = listen(s, 5);
    err_exit(r < 0, "cannot listen on socket: %s", strerror(r));
    
    printf("sock_bench: server on port %d\n", port);
    for (;;) {
        socklen_t socklen = sizeof(sin);
	
        int ss = accept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
	    printf("cannot accept client: %d\n", ss);
            continue;
        }
	
	if (threaded) {
#if JOS64
	    struct cobj_ref t;
	    r = thread_create(start_env->proc_container, &server,
			      (void*) (int64_t) ss, &t, "server");
	    if (r < 0) {
		printf("cannot spawn client thread: %s\n", e2s(r));
		close(ss);
	    } else {
		fd_give_up_privilege(ss);
	    }
#else 
	    pthread_t t;
	    r = pthread_create(&t, 0, server, (void *)(int64_t)ss);
#endif
	} else
	    server((void *) (int64_t)ss);	
    }
    return 0;
}
