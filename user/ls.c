#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

int
main(int ac, char **av)
{
    char *dname;
    if (ac == 1) {
	dname = "/";
    } else if (ac == 2) {
	dname = av[1];
    } else {
	printf("Usage: %s [dirname]\n", av[0]);
	return -1;
    }

    struct fs_inode dir;
    int r = fs_namei(dname, &dir);
    if (r < 0) {
	printf("cannot lookup %s: %s\n", dname, e2s(r));
	return r;
    }

    for (uint64_t n = 0; ; n++) {
	struct fs_dent de;
	r = fs_get_dent(dir, n, &de);
	if (r < 0) {
	    if (r == -E_NOT_FOUND)
		continue;
	    if (r != -E_RANGE)
		printf("fs_get_dent: %s", e2s(r));
	    break;
	}

	printf("%s\n", de.de_name);
    }
}
