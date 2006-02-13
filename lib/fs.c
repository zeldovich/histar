#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>

static int fs_debug = 0;

int
fs_get_root(uint64_t ct, struct fs_inode *rdirp)
{
    // no directory segment (yet?)
    rdirp->obj = COBJ(ct, ct);
    return 0;
}

int
fs_get_dent(struct fs_inode d, uint64_t n, struct fs_dent *e)
{
    int64_t slot_id = sys_container_get_slot_id(d.obj.object, n);
    if (slot_id < 0) {
	if (slot_id == -E_INVAL)
	    return -E_RANGE;
	if (slot_id == -E_NOT_FOUND)
	    return -E_NOT_FOUND;
	return slot_id;
    }

    e->de_inode.obj = COBJ(d.obj.object, slot_id);

    int r = sys_obj_get_name(e->de_inode.obj, &e->de_name[0]);
    if (r < 0)
	return r;

    if (fs_debug)
	cprintf("fs_get_dent: dir %ld obj %ld name %s\n",
		d.obj.object, slot_id, &e->de_name[0]);

    return 0;
}

int
fs_get_obj(struct fs_inode ino, struct cobj_ref *segp)
{
    *segp = ino.obj;
    return 0;
}

int
fs_lookup_one(struct fs_inode dir, const char *fn, struct fs_inode *o)
{
    if (fs_debug)
	cprintf("fs_lookup_one: dir %ld fn %s\n",
		dir.obj.object, fn);

    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *mnt = &start_env->fs_mtab.mtab_ent[i];
	if (mnt->mnt_dir.obj.object == dir.obj.object &&
	    !strcmp(&mnt->mnt_name[0], fn))
	{
	    *o = mnt->mnt_root;
	    return 0;
	}
    }

    struct fs_dent de;
    int n = 0;

    for (;;) {
	int r = fs_get_dent(dir, n++, &de);
	if (r < 0) {
	    if (r == -E_NOT_FOUND)
		continue;
	    if (r == -E_RANGE)
		return -E_NOT_FOUND;
	    return r;
	}

	if (!strcmp(fn, de.de_name)) {
	    *o = de.de_inode;
	    return 0;
	}
    }
}

int
fs_lookup_path(struct fs_inode dir, const char *pn, struct fs_inode *o)
{
    *o = dir;

    while (pn[0] != '\0') {
	const char *name_end = strchr(pn, '/');
	const char *next_pn = name_end ? name_end + 1 : "";
	size_t namelen = name_end ? (size_t) (name_end - pn) : strlen(pn);

	char fn[KOBJ_NAME_LEN];
	if (namelen >= sizeof(fn))
	    return -E_RANGE;

	strncpy(&fn[0], pn, namelen + 1);
	fn[namelen] = '\0';

	if (fn[0] != '\0') {
	    int r = fs_lookup_one(*o, &fn[0], o);
	    if (r < 0)
		return r;
	}

	pn = next_pn;
    }

    return 0;
}

int
fs_namei(const char *pn, struct fs_inode *o)
{
    struct fs_inode d = pn[0] == '/' ? start_env->fs_root : start_env->fs_cwd;
    return fs_lookup_path(d, pn, o);
}

int
fs_mkdir(struct fs_inode dir, const char *fn, struct fs_inode *o)
{
    int64_t r = sys_container_alloc(dir.obj.object, 0, fn);
    if (r < 0)
	return r;

    o->obj = COBJ(dir.obj.object, r);
    return 0;
}

int
fs_mount(struct fs_inode dir, const char *mnt_name, struct fs_inode root)
{
    for (int i = 0; i < FS_NMOUNT; i++) {
	if (start_env->fs_mtab.mtab_ent[i].mnt_name[0] == '\0') {
	    strncpy(&start_env->fs_mtab.mtab_ent[i].mnt_name[0],
		    mnt_name, KOBJ_NAME_LEN - 1);
	    start_env->fs_mtab.mtab_ent[i].mnt_dir = dir;
	    start_env->fs_mtab.mtab_ent[i].mnt_root = root;
	    return 0;
	}
    }

    return -E_NO_SPACE;
}
