#include <test/josenv.hh>
#include <test/disk.hh>

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

extern "C" {
#include <btree/btree.h>
#include <kern/freelist.h>
#include <kern/log.h>
#include <kern/disklayout.h>
}

#include <inc/error.hh>

#define errno_check(expr) \
    do {								\
	if ((expr) < 0)							\
	    throw basic_exception("%s: %s", #expr, strerror(errno));	\
    } while (0)

// Global freelist..
struct freelist freelist;

int
main(int ac, char **av)
try
{
    if (ac != 2) {
	printf("Usage: %s disk-file\n", av[0]);
	exit(-1);
    }

    int fd;
    errno_check(fd = open(av[1], O_RDWR));

    struct stat st;
    error_check(fstat(fd, &st));

    int n_sectors = st.st_size / 512;
    if (n_sectors * 512 < RESERVED_PAGES * 4096) {
	printf("Disk too small, need at least %d bytes\n", RESERVED_PAGES*4096);
	exit(-1);
    }

    disk_init(fd, n_sectors * 512);

    // Try to do something..
    log_init();
    btree_manager_init();
    freelist_init(&freelist, RESERVED_PAGES * 4096, n_sectors * 512 - RESERVED_PAGES * 4096);

    int log_size = log_flush();
    printf("flushed log: %d pages\n", log_size);

    printf("All done.\n");
} catch (std::exception &e) {
    printf("exception: %s\n", e.what());
}
