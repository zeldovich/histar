#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/mlt.h>

static int fs_debug = 0;
static int fs_label_debug = 0;

static struct ulabel *
fs_get_label(void)
{
    struct ulabel *l = label_get_current();
    if (l)
	label_change_star(l, l->ul_default);
    return l;
}

int
fs_get_root(uint64_t ct, struct fs_inode *rdirp)
{
    // no directory segment (yet?)
    rdirp->obj = COBJ(ct, ct);
    return 0;
}

static int
fs_get_dent_ct(struct fs_inode d, uint64_t n, struct fs_dent *e)
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

    return 0;
}

static int
fs_get_dent_mlt(struct fs_inode d, uint64_t n, struct fs_dent *e)
{
    char buf[MLT_BUF_SIZE];
    uint64_t ct;
    int r = sys_mlt_get(d.obj, n, 0, &buf[0], &ct);
    if (r < 0) {
	if (r == -E_NOT_FOUND)
	    return -E_RANGE;
	return r;
    }

    e->de_inode.obj = COBJ(ct, ct);
    snprintf(&e->de_name[0], sizeof(e->de_name), "%lu", ct);
    return 0;
}

int
fs_get_dent(struct fs_inode d, uint64_t n, struct fs_dent *e)
{
    int type = sys_obj_get_type(d.obj);
    if (type < 0)
	return type;

    // For the debugging cprintf further down
    e->de_name[0] = '\0';

    int r = -E_INVAL;
    if (type == kobj_container)
	r = fs_get_dent_ct(d, n, e);
    else if (type == kobj_mlt)
	r = fs_get_dent_mlt(d, n, e);

    if (fs_debug)
	cprintf("fs_get_dent: dir %ld r %d obj %ld name %s\n",
		d.obj.object, r, e->de_inode.obj.object,
		&e->de_name[0]);

    return r;
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

    // Simple MLT support
    if (!strcmp(fn, "@mlt")) {
	char blob[MLT_BUF_SIZE];
	uint64_t ct;
	int r = sys_mlt_get(dir.obj, 0, 0, &blob[0], &ct);
	if (r < 0) {
	    struct ulabel *l = label_get_current();
	    if (l == 0)
		return -E_NO_MEM;

	    label_change_star(l, l->ul_default);

	    if (fs_label_debug)
		cprintf("Creating MLT branch with label %s\n",
			label_to_string(l));

	    r = sys_mlt_put(dir.obj, l, &blob[0]);
	    label_free(l);
	    if (r < 0)
		return r;

	    r = sys_mlt_get(dir.obj, 0, 0, &blob[0], &ct);
	    if (r < 0)
		return r;
	}

	o->obj = COBJ(ct, ct);
	return 0;
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
    struct ulabel *l = fs_get_label();
    if (l == 0)
	return -E_NO_MEM;

    int64_t r = sys_container_alloc(dir.obj.object, l, fn);
    label_free(l);
    if (r < 0)
	return r;

    o->obj = COBJ(dir.obj.object, r);
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

int
fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f)
{
    // XXX not atomic
    int r = fs_lookup_one(dir, fn, f);
    if (r >= 0)
	return -E_EXISTS;

    struct ulabel *l = fs_get_label();
    if (l == 0)
	return -E_NO_MEM;

    int64_t id = sys_segment_create(dir.obj.object, 0, l, fn);

    if (fs_label_debug)
	cprintf("Creating file with label %s\n", label_to_string(l));

    label_free(l);
    if (id < 0)
	return id;

    f->obj = COBJ(dir.obj.object, id);
    return 0;
}

int
fs_remove(struct fs_inode f)
{
    return sys_obj_unref(f.obj);
}

void
fs_dirbase(char *pn, const char **dirname, const char **basename)
{
    char *slash = strrchr(pn, '/');
    if (slash == 0) {
	*dirname = "";
	*basename = pn;
    } else {
	*slash = '\0';
	*dirname = pn;
	*basename = slash + 1;
    }
}

int
fs_pwrite(struct fs_inode f, void *buf, uint64_t count, uint64_t off)
{
    uint64_t cursize;
    int r = fs_getsize(f, &cursize);
    if (r < 0)
	return r;

    uint64_t endpt = off + count;
    if (endpt > cursize)
	sys_segment_resize(f.obj, (endpt + PGSIZE - 1) / PGSIZE);

    char *map = 0;
    r = segment_map(f.obj, SEGMAP_READ | SEGMAP_WRITE, (void **) &map, 0);
    if (r < 0)
	return r;

    memcpy(&map[off], buf, count);
    segment_unmap(map);

    return 0;
}

int
fs_pread(struct fs_inode f, void *buf, uint64_t count, uint64_t off)
{
    uint64_t cursize;
    int r = fs_getsize(f, &cursize);
    if (r < 0)
	return r;

    uint64_t endpt = off + count;
    if (endpt > cursize)
	return -E_RANGE;

    char *map = 0;
    r = segment_map(f.obj, SEGMAP_READ, (void **) &map, 0);
    if (r < 0)
	return r;

    memcpy(buf, &map[off], count);
    segment_unmap(map);

    return 0;
}

int
fs_getsize(struct fs_inode f, uint64_t *len)
{
    int64_t npages = sys_segment_get_npages(f.obj);
    if (npages < 0)
	return npages;

    *len = npages * PGSIZE;
    return 0;
}
