#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <string.h>

void
fs_get_root(uint64_t ct, struct fs_inode *rdirp)
{
    rdirp->obj = COBJ(ct, ct);
}

int
fs_mount(struct cobj_ref fs_mtab_seg, struct fs_inode dir, 
	 const char *mnt_name, struct fs_inode root)
{
    struct fs_mount_table *mtab = 0;
    int r = segment_map(fs_mtab_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &mtab, 0, 0);
    if (r < 0)
	return r;

    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *ent = &mtab->mtab_ent[i];
	if (ent->mnt_name[0] == '\0') {
	    strncpy(&ent->mnt_name[0], mnt_name, KOBJ_NAME_LEN - 1);
	    ent->mnt_dir = dir;
	    ent->mnt_root = root;
	    segment_unmap(mtab);
	    return 0;
	}
    }

    segment_unmap(mtab);
    return -E_NO_SPACE;
}

void
fs_unmount(struct cobj_ref fs_mtab_seg, struct fs_inode dir, const char *mnt_name)
{
    struct fs_mount_table *mtab = 0;
    int r = segment_map(fs_mtab_seg, 0, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &mtab, 0, 0);
    if (r < 0) {
	cprintf("fs_unmount: cannot map: %s\n", e2s(r));
	return;
    }

    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *ent = &mtab->mtab_ent[i];
	if (ent->mnt_dir.obj.object == dir.obj.object &&
	    !strcmp(ent->mnt_name, mnt_name))
	{
	    ent->mnt_name[0] = '\0';
	}
    }

    segment_unmap(mtab);
}
