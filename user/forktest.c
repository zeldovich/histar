#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s fork-count\n", av[0]);
	exit(-1);
    }

    int num = atoi(av[1]);
    printf("Forking %d children..\n", num);

    for (int i = 0; i < num; i++) {
	pid_t pid = fork();
	if (pid < 0) {
	    printf("fork error: %s\n", strerror(errno));
	    continue;
	}

	if (pid == 0) {
	    printf("Hello from child.\n");
	    exit(0);
	}
    }

    printf("Done.\n");
    sleep(1);
}
