#include <stdio.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/fs.h>
#include <inc/fd.h>
#include <string.h>
#include <inc/stdio.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: %s dst\n", av[0]);
	return -1;
    }

    char *dst_pn = av[1];

    const char *dir, *fn;
    fs_dirbase(dst_pn, &dir, &fn);

    struct fs_inode dst_dir;
    int r = fs_namei(dir, &dst_dir);
    if (r < 0) {
	printf("cannot lookup destination directory %s: %s\n", dir, e2s(r));
	return r;
    }

    struct fs_inode dst;
    r = fs_create(dst_dir, fn, &dst);
    if (r < 0) {
	printf("cannot create destination file: %s\n", e2s(r));
	return r;
    }

    char buf[512];
    sprintf(&buf[0], "Hello world.\n");
    size_t cc = strlen(&buf[0]);

    r = fs_pwrite(dst, &buf[0], cc, 0);
    if (r < 0) {
	printf("cannot write: %s\n", e2s(r));
	return r;
    }

    return 0;
}
