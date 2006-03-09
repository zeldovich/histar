#include <inc/fs.h>
#include <inc/lib.h>
#include <string.h>
#include <inc/error.h>

int
fs_get_root(uint64_t ct, struct fs_inode *rdirp)
{
    // no directory segment (yet?)
    rdirp->obj = COBJ(ct, ct);
    return 0;
}

int
fs_get_obj(struct fs_inode ino, struct cobj_ref *segp)
{
    *segp = ino.obj;
    return 0;
}

int
fs_mount(struct fs_inode dir, const char *mnt_name, struct fs_inode root)
{
    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *ent = &start_env->fs_mtab.mtab_ent[i];
	if (ent->mnt_name[0] == '\0') {
	    strncpy(&ent->mnt_name[0], mnt_name, KOBJ_NAME_LEN - 1);
	    ent->mnt_dir = dir;
	    ent->mnt_root = root;
	    return 0;
	}
    }

    return -E_NO_SPACE;
}

void
fs_unmount(struct fs_inode dir, const char *mnt_name)
{
    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *ent = &start_env->fs_mtab.mtab_ent[i];
	if (ent->mnt_dir.obj.object == dir.obj.object &&
	    !strcmp(ent->mnt_name, mnt_name))
	{
	    ent->mnt_name[0] = '\0';
	}
    }
}
