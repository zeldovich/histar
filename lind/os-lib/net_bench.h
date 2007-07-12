#ifndef LINUX_ARCH_OS_LINUX_NET_BENCH_H
#define LINUX_ARCH_OS_LINUX_NET_BENCH_H

#define error_check(expr)				\
    do {						\
	int __r = (expr);				\
	if (__r < 0) {					\
	    fprintf(stderr, "%s:%u: %s: %s\n", 		\
		    __FILE__, __LINE__, #expr,          \
		    strerror(errno));	                \
            exit(-1);                                   \
        }                                               \
    } while (0)

#define xsocket linux_socket
#define xbind linux_bind
#define xlisten linux_listen
#define xaccept linux_accept
#define xread linux_read
#define xwrite linux_write
#define xconnect linux_connect
#define xclose linux_close

int nbread(int s, char *b, int count);
int nbwrite(int s, char *b, int count);

#endif
