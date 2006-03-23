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
    if (ac == 1) {
	for (int i = 0; i < FS_NMOUNT; i++) {
	    struct fs_mtab_ent *mtab = &fs_mtab->mtab_ent[i];
	    if (mtab->mnt_name[0])
		printf("<%ld.%ld>: %s -> <%ld.%ld>\n",
		       mtab->mnt_dir.obj.container, mtab->mnt_dir.obj.object,
		       mtab->mnt_name,
		       mtab->mnt_root.obj.container, mtab->mnt_root.obj.object);
	}
    } else if (ac == 3) {
	const char *mntdir = av[1];
	const char *target = av[2];

	uint64_t ct;
	struct fs_inode mntdir_ino;
	int r = fs_namei(mntdir, &mntdir_ino);
	if (r < 0) {
	    r = strtou64(mntdir, 0, 10, &ct);
	    if (r < 0) {
		printf("cannot parse existing directory or container: %s\n", e2s(r));
		return -1;
	    }
	} else {
	    ct = mntdir_ino.obj.object;
	}

	struct fs_inode dir, root;
	fs_get_root(ct, &root);

	char *pn2 = strdup(target);
	const char *dirname, *fname;
	fs_dirbase(pn2, &dirname, &fname);

	r = fs_namei(dirname, &dir);
	if (r < 0) {
	    printf("fs_namei(%s): %s\n", dirname, e2s(r));
	    return -1;
	}

	r = fs_mount(dir, fname, root);
	if (r < 0)
	    printf("fs_mount: %s\n", e2s(r));
    } else {
	printf("Usage: mount\n");
	printf("       mount <existing-directory> <new-directory>\n");
	printf("       mount <container> <new-directory>\n");
    }
}
