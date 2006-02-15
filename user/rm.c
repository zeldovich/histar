#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s pathname\n", av[0]);
	return -1;
    }

    const char *pn = av[1];
    struct fs_inode f;
    int r = fs_namei(pn, &f);
    if (r < 0) {
	printf("cannot lookup %s: %s\n", pn, e2s(r));
	return r;
    }

    r = fs_remove(f);
    if (r < 0) {
	printf("cannot remove %s: %s\n", pn, e2s(r));
	return r;
    }

    return 0;
}
