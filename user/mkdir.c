#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s pathname\n", av[0]);
	return -1;
    }

    char *pn = av[1];

    int r = mkdir(pn, 0777);
    if (r < 0)
	printf("cannot mkdir %s: %s\n", pn, strerror(errno));

    return r;
}
