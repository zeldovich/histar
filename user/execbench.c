#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s count\n", av[0]);
	exit(-1);
    }

    uint32_t count = atoi(av[1]);
    if (count == 0) {
	printf("Bad count\n");
	exit(-1);
    }

    struct timeval start;
    gettimeofday(&start, 0);

    uint32_t i;
    for (i = 0; i < count; i++)
	if (system("/bin/true") < 0)
	    perror("system");

    struct timeval end;
    gettimeofday(&end, 0);

    uint64_t diff_usec =
	(end.tv_sec - start.tv_sec) * 1000000 +
	end.tv_usec - start.tv_usec;
    printf("Total time: %ld usec\n", diff_usec);
    printf("usec per rtt: %ld\n", diff_usec / count);
}
