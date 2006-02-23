#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/mlt.h>
#include <inc/pthread.h>

static int fs_debug = 0;
static int fs_label_debug = 0;

struct fs_dirslot {
    uint64_t id;
    uint8_t inuse;
    char name[KOBJ_NAME_LEN];
};

struct fs_directory {
    pthread_mutex_t lock;
    struct fs_dirslot slots[0];
};

struct fs_opendir {
    struct fs_inode ino;

    int writable;
    struct cobj_ref dseg;
    struct fs_directory *dir;
    void *dir_end;
};

static int
fs_dir_init(struct fs_inode dir, struct ulabel *l)
{
    struct cobj_ref dseg;
    return segment_alloc(dir.obj.object, PGSIZE, &dseg, 0, l, "directory segment");
}

static int
fs_dir_open(struct fs_opendir *s, struct fs_inode dir, int writable)
{
    s->writable = writable;
    s->ino = dir;

    int64_t r = sys_container_get_slot_id(dir.obj.object, 0);
    if (r < 0)
	return r;

    s->dseg = COBJ(dir.obj.object, r);

    char name[KOBJ_NAME_LEN];
    r = sys_obj_get_name(s->dseg, &name[0]);
    if (r < 0)
	return r;
    if (strcmp(&name[0], "directory segment"))
	return -E_NOT_FOUND;

    s->dir = 0;
    uint64_t perm = SEGMAP_READ;
    if (s->writable)
	perm |= SEGMAP_WRITE;

    uint64_t dirsize;
    r = segment_map(s->dseg, perm, (void **) &s->dir, &dirsize);
    if (r < 0)
	return r;

    s->dir_end = ((void *) s->dir) + dirsize;

    if (s->writable)
	pthread_mutex_lock(&s->dir->lock);
    return 0;
}

static int
fs_dir_grow(struct fs_opendir *s)
{
    assert(s->writable);

    int64_t curpages = sys_segment_get_npages(s->dseg);
    if (curpages < 0)
	return curpages;

    int r = segment_unmap(s->dir);
    if (r < 0)
	return r;

    r = sys_segment_resize(s->dseg, curpages + 1);
    if (r < 0)
	return r;

    uint64_t dirsize;
    r = segment_map(s->dseg, SEGMAP_READ | SEGMAP_WRITE,
		    (void **) &s->dir, &dirsize);
    if (r < 0)
	return r;

    s->dir_end = ((void *) s->dir) + dirsize;
    return 0;
}

static void
fs_dir_close(struct fs_opendir *s)
{
    if (s->writable)
	pthread_mutex_unlock(&s->dir->lock);
    segment_unmap(s->dir);
}

static void
fs_dir_remove(struct fs_opendir *s, uint64_t id)
{
    struct fs_dirslot *slot;

    for (slot = &s->dir->slots[0]; (slot + 1) <= (struct fs_dirslot *) s->dir_end; slot++)
	if (slot->inuse && slot->id == id)
	    slot->inuse = 0;
}

static int
fs_dir_lookup(struct fs_opendir *s, const char *fn, uint64_t *idp)
{
    struct fs_dirslot *slot;

    for (slot = &s->dir->slots[0]; (slot + 1) <= (struct fs_dirslot *) s->dir_end; slot++) {
	if (slot->inuse && !strcmp(slot->name, fn)) {
	    *idp = slot->id;
	    return 0;
	}
    }

    return -E_NOT_FOUND;
}

static int __attribute__((unused))
fs_dir_getslot(struct fs_opendir *s, uint32_t n, char *fn, uint64_t *idp)
{
    struct fs_dirslot *slot = &s->dir->slots[n];
    if (slot + 1 > (struct fs_dirslot *) s->dir_end)
	return -E_RANGE;
    if (slot->inuse == 0)
	return -E_NOT_FOUND;

    memcpy(fn, &slot->name[0], KOBJ_NAME_LEN);
    *idp = slot->id;
    return 0;
}

static int
fs_dir_put(struct fs_opendir *s, const char *fn, uint64_t id)
{
    uint64_t old_id;
    int r = fs_dir_lookup(s, fn, &old_id);
    if (r >= 0)
	return -E_EXISTS;

    struct fs_dirslot *slot;

retry:
    for (slot = &s->dir->slots[0]; (slot + 1) <= (struct fs_dirslot *) s->dir_end; slot++) {
	if (!slot->inuse) {
	    strncpy(&slot->name[0], fn, KOBJ_NAME_LEN - 1);
	    slot->id = id;
	    slot->inuse = 1;
	    return 0;
	}
    }

    r = fs_dir_grow(s);
    if (r < 0)
	return r;

    goto retry;
}

static struct ulabel *
fs_get_label(void)
{
    struct ulabel *l = label_get_current();
    if (l)
	label_change_star(l, l->ul_default);
    return l;
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
	int retry_count = 0;
	struct ulabel *lcur, *lslot;

retry:
	lcur = label_get_current();
	if (lcur == 0)
	    return -E_NO_MEM;

	lslot = label_alloc();
	if (lslot == 0)
	    return -E_NO_MEM;

	int r = -1;

	for (uint64_t slot = 0; r < 0; slot++) {
	    r = sys_mlt_get(dir.obj, slot, lslot, &blob[0], &ct);
	    if (r < 0) {
		if (r == -E_NOT_FOUND)
		    break;
		return r;
	    }

	    r = label_compare(lcur, lslot, label_leq_starlo);
	    if (r == 0)
		r = label_compare(lslot, lcur, label_leq_starhi);
	}

	if (r < 0) {
	    if (retry_count++ > 0)
		return -E_INVAL;

	    label_change_star(lcur, lcur->ul_default);

	    if (fs_label_debug)
		cprintf("Creating MLT branch with label %s\n",
			label_to_string(lcur));

	    r = sys_mlt_put(dir.obj, lcur, &blob[0]);
	    if (r < 0)
		return r;

	    label_free(lslot);
	    label_free(lcur);
	    goto retry;
	}

	label_free(lslot);
	label_free(lcur);

	o->obj = COBJ(ct, ct);
	return 0;
    }

    struct fs_opendir od;
    int r = fs_dir_open(&od, dir, 0);
    if (r >= 0) {
	uint64_t id;
	r = fs_dir_lookup(&od, fn, &id);
	if (r < 0) {
	    fs_dir_close(&od);
	    return r;
	}

	o->obj = COBJ(dir.obj.object, id);
	fs_dir_close(&od);
	return 0;
    }

    struct fs_dent de;
    int n = 0;

    for (;;) {
	r = fs_get_dent(dir, n++, &de);
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
    struct fs_opendir od;
    int use_od = 1;
    int r = fs_dir_open(&od, dir, 1);
    if (r < 0)
	use_od = 0;

    struct ulabel *l = fs_get_label();
    if (l == 0) {
	if (use_od)
	    fs_dir_close(&od);
	return -E_NO_MEM;
    }

    int64_t id = sys_container_alloc(dir.obj.object, l, fn);
    label_free(l);
    if (id < 0) {
	if (use_od)
	    fs_dir_close(&od);
	return id;
    }

    o->obj = COBJ(dir.obj.object, id);

    r = fs_dir_init(*o, l);
    if (r < 0) {
	fs_dir_close(&od);
	return r;
    }

    if (use_od) {
	r = fs_dir_put(&od, fn, id);
	if (r < 0) {
	    sys_obj_unref(o->obj);
	    fs_dir_close(&od);
	    return r;
	}

	fs_dir_close(&od);
    }

    return 0;
}

int
fs_mkmlt(struct fs_inode dir, const char *fn, struct fs_inode *o)
{
    struct fs_opendir od;
    int r = fs_dir_open(&od, dir, 1);
    if (r < 0)
	return r;

    int64_t id = sys_mlt_create(dir.obj.object, fn);
    if (id < 0) {
	fs_dir_close(&od);
	return id;
    }

    o->obj = COBJ(dir.obj.object, id);

    r = fs_dir_put(&od, fn, id);
    if (r < 0) {
	sys_obj_unref(o->obj);
	fs_dir_close(&od);
	return r;
    }

    fs_dir_close(&od);
    return 0;
}

int
fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f)
{
    struct fs_opendir od;
    int r = fs_dir_open(&od, dir, 1);
    if (r < 0)
	return r;

    struct ulabel *l = fs_get_label();
    if (l == 0) {
	fs_dir_close(&od);
	return -E_NO_MEM;
    }

    int64_t id = sys_segment_create(dir.obj.object, 0, l, fn);

    if (fs_label_debug)
	cprintf("Creating file with label %s\n", label_to_string(l));

    label_free(l);
    if (id < 0) {
	fs_dir_close(&od);
	return id;
    }

    r = fs_dir_put(&od, fn, id);
    if (r < 0) {
	sys_obj_unref(COBJ(dir.obj.object, id));
	fs_dir_close(&od);
	return r;
    }

    f->obj = COBJ(dir.obj.object, id);
    fs_dir_close(&od);
    return 0;
}

int
fs_remove(struct fs_inode f)
{
    struct fs_inode dir = { COBJ(f.obj.container, f.obj.container) };
    struct fs_opendir od;
    int r = fs_dir_open(&od, dir, 1);
    if (r < 0)
	return r;

    r = sys_obj_unref(f.obj);
    if (r < 0) {
	fs_dir_close(&od);
	return r;
    }

    fs_dir_remove(&od, f.obj.object);
    fs_dir_close(&od);
    return 0;
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
