extern "C" {
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <wait.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
}

#include <inc/netbench.hh>
#include <inc/errno.hh>

static const int default_requests = 100;
static const int default_clients = 1;

static int byte_count = 150;
static int iter_count = 5;

static int 
tcp_connect(struct sockaddr_in *addr)
{
    int sock;
    errno_check(sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    errno_check(connect(sock, (struct sockaddr *)addr, sizeof(*addr)));
    return sock;
}

static int 
http_request(int s, const char *host, int port)
{
    static char buf[4096];

    if (byte_count) {
	try {
	    for (int i = 0; i < iter_count; i++) {
		nb_read(s, buf, byte_count);
		nb_write(s, buf, byte_count);
	    }
	} catch (std::exception &e) {
	    printf("client error: %s\n", e.what());
	}
    } 
    close(s);        
}

uint32_t completed = 0;

static void
timeout(int signo)
{
    fprintf(stderr, "time limit up, completed %d!\n", completed);
    fprintf(stdout, "%d\n", completed);
    fflush(0);
    kill(0, SIGQUIT);
}

int 
main(int ac, char **av)
{
    int port;
    const char *host;
    uint32_t requests = default_requests;
    int clients = default_clients;
    int timelimit = 0;

    if (ac < 3) {
	fprintf(stderr, "Usage: %s host port [-r requests | -c clients | -l time-limit | -b bytes | -i iters]\n", av[0]);
	exit(-1);
    }

    setpgrp();

    host = av[1];
    port = atoi(av[2]);

    int c;
    while ((c = getopt(ac, av, "r:c:l:b:i:")) != -1) {
	switch(c) {
	case 'r':
	    requests = atoi(optarg);
	    break;
	case 'c':
	    clients = atoi(optarg);
	    break;
	case 'l':
	    timelimit = atoi(optarg);
	    break;
	case 'b':
	    byte_count = atoi(optarg);
	    break;
	case 'i':
	    iter_count = atoi(optarg);
	    break;
	}
    }
    
    struct hostent *hp;
    struct sockaddr_in addr;
    
    if(!(hp = gethostbyname(host))) {
	printf("gethostbyname: couldn't resolve host\n");
	exit(1);
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr = *((struct in_addr*) hp->h_addr_list[0]);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    int fd[2];
    errno_check(pipe(fd));

    if (timelimit)
	requests = ~0;
    
    int i = 0;
    for (; i < clients; i++) {
	pid_t p;
	if (p = fork()) {
	    fprintf(stderr, "%d (%d) started...\n", i, p);
	    continue;
	}
	
	int log = fd[1];
	close(fd[0]);
	
	if (timelimit) {
	    // wait so all processes can be forked
	    kill(getpid(), SIGSTOP);
	}
	
	uint32_t i = 0;
	for (; i < requests; i++) {
	    int sock = tcp_connect(&addr);
	    http_request(sock, host, port);
	    if (timelimit)
		write(log, "*", 1);
	    close(sock);
	}
	return 0;
    }
    close(fd[1]);
    
    if (timelimit) {
	// make sure all processes have SIGSTOP
	usleep(1000000);
	fprintf(stderr, "starting clients...\n");

	signal(SIGALRM, &timeout);
	alarm(timelimit);
	kill(0, SIGCONT);

	// count connections for timelimit seconds
	for (;;) {
	    char a;
	    int r = read(fd[0], &a, 1);
	    if (r)
		completed++;
	}
    }

    for (i = 0; i < clients; i++) {
	pid_t p;
	errno_check(p = wait(0));
	fprintf(stderr, "%d (%d) terminated!\n", i, p);
    }
    return 0;
}
