extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/mlt.h>
#include <inc/pthread.h>
}

#include <inc/error.hh>
#include <inc/scopeguard.hh>

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

class missing_dir_segment : public error {
public:
    missing_dir_segment(int r, const char *m) : error(r, "%s", m) {}
};

class fs_opendir {
public:
    fs_opendir(const struct fs_inode &dir, int writable)
	: ino_(dir), writable_(writable)
    {
	int64_t r = sys_container_get_slot_id(dir.obj.object, 0);
	if (r < 0)
	    throw missing_dir_segment(r, "sys_container_get_slot_id");

	dseg_ = COBJ(dir.obj.object, r);

	char name[KOBJ_NAME_LEN];
	r = sys_obj_get_name(dseg_, &name[0]);
	if (r < 0)
	    throw missing_dir_segment(r, "sys_obj_get_name");
	if (strcmp(&name[0], "directory segment"))
	    throw missing_dir_segment(-E_NOT_FOUND, &name[0]);

	dir_ = 0;
	uint64_t perm = SEGMAP_READ;
	if (writable_)
	    perm |= SEGMAP_WRITE;

	uint64_t dirsize;
	r = segment_map(dseg_, perm, (void **) &dir_, &dirsize);
	if (r < 0)
	    throw error(r, "segment_map");

	dir_end_ = ((char *) dir_) + dirsize;

	if (writable_)
	    pthread_mutex_lock(&dir_->lock);
    }

    ~fs_opendir() {
	if (writable_)
	    pthread_mutex_unlock(&dir_->lock);
	segment_unmap(dir_);
    }

    void remove(uint64_t id) {
	struct fs_dirslot *slot;

	for (slot = &dir_->slots[0]; (slot + 1) <= (struct fs_dirslot *) dir_end_; slot++)
	    if (slot->inuse && slot->id == id)
		slot->inuse = 0;
    }

    int lookup(const char *fn, uint64_t *idp) {
	struct fs_dirslot *slot;

	for (slot = &dir_->slots[0]; (slot + 1) <= (struct fs_dirslot *) dir_end_; slot++) {
	    if (slot->inuse && !strcmp(slot->name, fn)) {
		*idp = slot->id;
		return 0;
	    }
	}

	return -E_NOT_FOUND;
    }

    void put(const char *fn, uint64_t id) {
	uint64_t old_id;
	int r = lookup(fn, &old_id);
	if (r >= 0)
	    throw error(-E_EXISTS, "file already exists");

	for (;;) {
	    struct fs_dirslot *slot;
	    for (slot = &dir_->slots[0];
		 (slot + 1) <= (struct fs_dirslot *) dir_end_;
		 slot++)
	    {
		if (!slot->inuse) {
		    strncpy(&slot->name[0], fn, KOBJ_NAME_LEN - 1);
		    slot->id = id;
		    slot->inuse = 1;
		    return;
		}
	    }

	    grow();
	}
    }

    int getslot(uint32_t n, char *fn, uint64_t *idp) {
	struct fs_dirslot *slot = &dir_->slots[n];
	if (slot + 1 > (struct fs_dirslot *) dir_end_)
	    return -E_RANGE;
	if (slot->inuse == 0)
	    return -E_NOT_FOUND;

	memcpy(fn, &slot->name[0], KOBJ_NAME_LEN);
	*idp = slot->id;
	return 0;
    }

private:
    void grow() {
	assert(writable_);

	int64_t curpages = sys_segment_get_npages(dseg_);
	if (curpages < 0)
	    throw error(curpages, "sys_segment_get_npages");

	int r = segment_unmap(dir_);
	if (r < 0)
	    throw error(r, "segment_unmap");

	r = sys_segment_resize(dseg_, curpages + 1);
	if (r < 0)
	    throw error(r, "sys_segment_resize");

	uint64_t dirsize;
	r = segment_map(dseg_, SEGMAP_READ | SEGMAP_WRITE,
			(void **) &dir_, &dirsize);
	if (r < 0)
	    throw error(r, "segment_map");

	dir_end_ = ((char *) dir_) + dirsize;
    }

    struct fs_inode ino_;

    int writable_;
    struct cobj_ref dseg_;
    struct fs_directory *dir_;
    void *dir_end_;
};

static int
fs_dir_init(struct fs_inode dir, struct ulabel *l)
{
    struct cobj_ref dseg;
    return segment_alloc(dir.obj.object, PGSIZE, &dseg, 0, l, "directory segment");
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
    uint8_t buf[MLT_BUF_SIZE];
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

static int
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
	uint8_t blob[MLT_BUF_SIZE];
	uint64_t ct;
	int retry_count = 0;
	struct ulabel *lcur, *lslot;

retry:
	lcur = label_get_current();
	if (lcur == 0)
	    return -E_NO_MEM;

	scope_guard<struct ulabel *> lf1(label_free, lcur);

	lslot = label_alloc();
	if (lslot == 0)
	    return -E_NO_MEM;
	scope_guard<struct ulabel *> lf2(label_free, lslot);

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

	    goto retry;
	}

	o->obj = COBJ(ct, ct);
	return 0;
    }

    try {
	fs_opendir od(dir, 0);

	uint64_t id;
	int r = od.lookup(fn, &id);
	if (r < 0)
	    return r;

	o->obj = COBJ(dir.obj.object, id);
	return 0;
    } catch (missing_dir_segment &e) {}

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

static int
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
    scope_guard<struct ulabel *> lf(label_free, l);

    int64_t id = sys_container_alloc(dir.obj.object, l, fn);
    if (id < 0)
	return id;

    o->obj = COBJ(dir.obj.object, id);
    int r = fs_dir_init(*o, l);
    if (r < 0)
	return r;

    try {
	fs_opendir od(dir, 1);
	od.put(fn, id);
    } catch (missing_dir_segment &e) {
    } catch (error &e) {
	sys_obj_unref(o->obj);
	return e.err();
    }

    return 0;
}

int
fs_mkmlt(struct fs_inode dir, const char *fn, struct fs_inode *o)
{
    int64_t id = sys_mlt_create(dir.obj.object, fn);
    if (id < 0)
	return id;

    o->obj = COBJ(dir.obj.object, id);

    try {
	fs_opendir od(dir, 1);
	od.put(fn, id);
    } catch (error &e) {
	sys_obj_unref(o->obj);
	return e.err();
    }

    return 0;
}

int
fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f)
{
    struct ulabel *l = fs_get_label();
    if (l == 0)
	return -E_NO_MEM;
    scope_guard<struct ulabel *> lf(label_free, l);

    if (fs_label_debug)
	cprintf("Creating file with label %s\n", label_to_string(l));

    int64_t id = sys_segment_create(dir.obj.object, 0, l, fn);
    if (id < 0)
	return id;

    f->obj = COBJ(dir.obj.object, id);

    try {
	fs_opendir od(dir, 1);
	od.put(fn, id);
    } catch (error &e) {
	sys_obj_unref(f->obj);
	return e.err();
    }

    return 0;
}

int
fs_remove(struct fs_inode f)
{
    try {
	struct fs_inode dir = { COBJ(f.obj.container, f.obj.container) };
	fs_opendir od(dir, 1);

	int r = sys_obj_unref(f.obj);
	if (r < 0)
	    return r;

	od.remove(f.obj.object);
    } catch (error &e) {
	return e.err();
    }

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
