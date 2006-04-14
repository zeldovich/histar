#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
    	printf("Usage: %s command\n", av[0]);
    	exit(-1);
    }

    char *n;
    if ((n = strrchr(av[1], '/')) == 0)
        n = av[1];
    else
        n++;

    struct timeval start;
    gettimeofday(&start, 0);

    pid_t pid = fork();
	if (pid == 0) {
	    execl(av[1], n, 0);
	    perror("exec");
	    exit(-1);
    }
    int status;
	waitpid(pid, &status, 0);

    struct timeval end;
    gettimeofday(&end, 0);

    uint64_t diff_usec =
	(end.tv_sec - start.tv_sec) * 1000000 +
	end.tv_usec - start.tv_usec;
    printf("Total time: %ld usec\n", diff_usec);
}
