extern "C" {
#include <inc/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
}

#include <inc/error.hh>
#include <inc/spawn.hh>

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

    fs_inode bin;
    assert(0 == fs_namei("/bin/true", &bin));

    uint32_t i;
    for (i = 0; i < count; i++) {
	try {
	    child_process cp =
		spawn(start_env->shared_container, bin,
		      0, 1, 2,
		      0, 0,
		      0, 0,
		      0, 0, 0, 0);

	    int64_t ec;
	    process_wait(&cp, &ec);
	    if (ec)
		throw basic_exception("Funny error-code %ld\n", ec);
	} catch (std::exception &e) {
	    printf("spawn: %s\n", e.what());
	}
    }

    struct timeval end;
    gettimeofday(&end, 0);

    uint64_t diff_usec =
	(end.tv_sec - start.tv_sec) * 1000000 +
	end.tv_usec - start.tv_usec;
    printf("Total time: %ld usec\n", diff_usec);
    printf("usec per rtt: %ld\n", diff_usec / count);
}
