#include <inc/lib.h>
#include <inc/stdio.h>
#include <inc/string.h>

static void
usage(const char *n)
{
    printf("Usage: %s container object\n", n);
}

int
main(int ac, char **av)
{
    if (ac != 3) {
	usage(av[0]);
	return -1;
    }

    struct cobj_ref as;
    if (strtoull(av[1], 0, 10, &as.container) < 0) {
	usage(av[0]);
	return -1;
    }

    if (strtoull(av[2], 0, 10, &as.object) < 0) {
	usage(av[0]);
	return -1;
    }

    segment_as_print(as);
    return 0;
}
