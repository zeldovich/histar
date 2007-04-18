#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef JOS_USER
#include <inc/syscall.h>
#endif

static void __attribute__((noreturn))
xperror(const char *msg)
{
    perror(msg);
    exit(-1);
}

int
main(int ac, char **av)
{
    if (ac != 2 && ac != 3) {
	printf("Usage: %s rttcount [cflush]\n", av[0]);
	exit(-1);
    }

    uint32_t count = atoi(av[1]);
    if (count == 0) {
	printf("Bad count\n");
	exit(-1);
    }

    int cflush = 0;
    if (ac == 3)
	cflush = atoi(av[2]);

    printf("RTT count:   %d\n", count);
    printf("Cache flush: %d\n", cflush);

    int to_worker[2];
    int from_worker[2];
    if (pipe(to_worker) < 0 || pipe(from_worker) < 0)
	xperror("pipe");

    pid_t pid = fork();

#ifdef JOS_USER
    sys_self_set_cflush(cflush);
#endif

    if (pid == 0) {
	close(to_worker[1]);
	close(from_worker[0]);

	for (;;) {
	    uint64_t v;
	    ssize_t cc = read(to_worker[0], &v, sizeof(v));
	    if (cc < 0)
		xperror("read");

	    if (cc == 0)
		exit(0);

	    v++;
	    if (write(from_worker[1], &v, sizeof(v)) < 0)
		xperror("write");
	}
    }

    uint64_t v = 0;
    uint32_t i;

    struct timeval start;
    gettimeofday(&start, 0);

    for (i = 0; i < count; i++) {
	if (write(to_worker[1], &v, sizeof(v)) < 0)
	    xperror("write");

	if (read(from_worker[0], &v, sizeof(v)) < 0)
	    xperror("read");
    }

    struct timeval end;
    gettimeofday(&end, 0);

    if (v != count)
	printf("value mismatch: %ld != %d\n", v, count);

    uint64_t diff_usec =
	(end.tv_sec - start.tv_sec) * 1000000 +
	end.tv_usec - start.tv_usec;
    printf("Total time: %ld usec\n", diff_usec);
    printf("usec per rtt: %ld\n", diff_usec / count);
}
