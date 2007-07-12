#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <linuxsyscall.h>
#include "net_bench.h"

void
bench_server(unsigned short port)
{
    int s;
    struct sockaddr_in sin;
    
    error_check(s = xsocket(AF_INET, SOCK_STREAM, 0));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(port);
    error_check(xbind(s, (struct sockaddr *)&sin, sizeof(sin)));
    error_check(xlisten(s, 5));
    
    for (;;) {
        int ss, i, socklen;
	printf("* accepting...\n");
	
	socklen = sizeof(sin);
	ss = xaccept(s, (struct sockaddr *)&sin, &socklen);
        if (ss < 0) {
            fprintf(stderr, "cannot accept client: %s\n", strerror(errno));
            continue;
        }
    
	for (i = 0; i < 10; i++) {
	    char buf[1024];
	    int r;
	    r = nbread(ss, buf, sizeof(buf));
	    if (r == 0)
		break;
	    r = nbwrite(ss, buf, r);
	    if (r == 0)
		break;
	}
	xclose(ss);
    }
}
