#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

static void __attribute__((noreturn))
xperror(const char *msg)
{
    perror(msg);
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac < 3) {
	printf("Usage: %s tcp-port program-to-run [program-args]\n", av[0]);
	return -1;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
	xperror("socket");

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(atoi(av[1]));
    if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0)
	xperror("bind");
    if (listen(s, 5) < 0)
	xperror("listen");

    for (;;) {
	socklen_t len = sizeof(sin);
	int c = accept(s, (struct sockaddr *) &sin, &len);
	if (c < 0) {
	    perror("accept");
	    continue;
	}

	pid_t child = fork();
	if (child < 0) {
	    perror("fork");
	    close(c);
	} else if (child) {
	    close(c);
	} else {
	    close(s);
	    dup2(c, 0);
	    dup2(c, 1);
	    close(c);
	    execv(av[2], &av[2]);
	    xperror("execve");
	}
    }
}
