#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

#include <stdio.h>
#include <unistd.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s pathname\n", av[0]);
	return -1;
    }

    const char *pn = av[1];
    int r = unlink(pn);
    if (r < 0) {
	printf("cannot remove %s: %s\n", pn, e2s(r));
	return r;
    }

    return 0;
}
