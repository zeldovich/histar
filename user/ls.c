#include <stdio.h>
#include <inc/stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>

int
main(int ac, char **av)
{
    int r;
    struct fs_inode dir;

    if (ac == 1) {
	dir = start_env->fs_cwd;
    } else if (ac == 2) {
	const char *dname = av[1];
	r = fs_namei(dname, &dir);
	if (r < 0) {
	    printf("cannot lookup %s: %s\n", dname, e2s(r));
	    return r;
	}
    } else {
	printf("Usage: %s [dirname]\n", av[0]);
	return -1;
    }

    struct fs_readdir_state s;
    r = fs_readdir_init(&s, dir);
    if (r < 0) {
	printf("fs_readdir_init: %s\n", e2s(r));
	return r;
    }

    for (;;) {
	struct fs_dent de;
	r = fs_readdir_dent(&s, &de, 0);
	if (r < 0) {
	    printf("fs_readdir_dent: %s\n", e2s(r));
	    return r;
	}
	if (r == 0)
	    break;

	printf("%s\n", de.de_name);
    }
}
