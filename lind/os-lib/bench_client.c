#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <linuxsyscall.h>
#include "net_bench.h"

int
nbread(int s, char *b, int count)
{
    int ret = count;
    while (count) {
	int r = xread(s, b, count);
	if (r < 0 || r == 0)
	    return 0;
	count -= r;
	b += r;
    }
    return ret;
}

int
nbwrite(int s, char *b, int count)
{
    int ret = count;
    while (count) {
	int r = xwrite(s, b, count);
	if (r < 0 || r == 0)
	    return 0;
	count -= r;
	b += r;
    }
    return ret;
}

int
bench_client(char *ip, unsigned short port)
{
    int s, i;
    struct sockaddr_in sin;
    struct timeval tv0, tv1, tv2;
    
    error_check(s = xsocket(AF_INET, SOCK_STREAM, 0));

    
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port = htons(port);
    error_check(xconnect(s, (struct sockaddr *)&sin, sizeof(sin)));
    
    
    gettimeofday(&tv0, 0);
    for (i = 0; i < 10; i++) {
	char buf[1024];
	int r;
	r = nbwrite(s, buf, sizeof(buf));
	if (r == 0)
	    break;
	r = nbread(s, buf, sizeof(buf));
	if (r == 0)
	    break;
    }
    gettimeofday(&tv1, 0);
    timersub(&tv1, &tv0, &tv2);
    
    printf("sec %ld usec %ld\n", tv2.tv_sec, tv2.tv_usec);
    
    xclose(s);
    return 0;
}
