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
