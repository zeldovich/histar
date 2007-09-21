#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/fs.h>
#include <inc/fd.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>

int
main(int ac, char **av)
{
    if (ac != 2) {
	printf("Usage: umount <dirname>\n");
	return -1;
    }

    const char *target = av[1];

    char *pn2 = strdup(target);
    const char *dirname, *fname;
    fs_dirbase(pn2, &dirname, &fname);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0) {
	printf("fs_namei(%s): %s\n", dirname, e2s(r));
	return -1;
    }

    fs_unmount(start_env->fs_mtab_seg, dir, fname);
    return 0;
}
