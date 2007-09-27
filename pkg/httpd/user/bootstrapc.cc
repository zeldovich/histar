extern "C" {
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/fs.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
}

#include <inc/error.hh>
#include <inc/errno.hh>
#include <inc/nethelper.hh>

int
main(int ac, char **av)
{
    static uint16_t port = 1234;
    if (ac < 3) {
	printf("usage: %s ip req [suffix]\n", av[0]);
	return -1;
    }
    
    tcpconn conn(av[1], port);
    conn.write(av[2], strlen(av[2]));
    conn.write("\r\n", 2);
    lineparser lp(&conn);

    const char *suffix = 0;
    if (ac > 3)
	suffix = av[3];

    for (;;) {
	const char *fn;
	if (!(fn = lp.read_line()))
	    break;
	
	char buf[4096] = "/www/";
	strcat(buf, fn);
	if (suffix)
	    strcat(buf, suffix);

	int fd = open(buf, O_RDWR | O_CREAT | O_TRUNC, 0666);
	
	printf("creating %s...\n", buf);

	uint32_t count = atoi(lp.read_line());
	assert(sizeof(buf) > count);

	uint32_t cc = count;
	char *ptr = buf;
	while (cc) {
	    int r = lp.read(ptr, cc) ;
	    ptr += r;
	    cc -= r;
	}
	*ptr = 0;

	write(fd, buf, count);
	printf("wrote %d bytes!\n", count);
	
	close(fd);
    }
    return 0;
}
