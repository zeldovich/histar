#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s src dst\n", av[0]);
	return -1;
    }

    const char *src = av[1];
    const char *dst = av[2];
    int r = rename(src, dst);
    if (r < 0) {
	printf("cannot move %s to %s: %s\n", src, dst, strerror(errno));
	return r;
    }

    return 0;
}
