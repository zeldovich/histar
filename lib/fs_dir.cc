extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/mlt.h>
#include <inc/pthread.h>
#include <inc/declassify.h>
#include <inc/gateparam.h>

#include <string.h>
}

#include <inc/gateclnt.hh>
#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>

static int fs_debug = 0;

enum { type_cache_size = 16 };
static int type_cache_next;
static struct {
    uint64_t obj;
    int type;
} type_cache[type_cache_size];;

static fs_dir *
fs_dir_open(fs_inode dir, bool writable)
{
    int type = -1;
    for (uint32_t i = 0; i < type_cache_size; i++)
	if (type_cache[i].obj == dir.obj.object)
	    type = type_cache[i].type;

    if (type < 0) {
	type = sys_obj_get_type(dir.obj);
	if (type < 0)
	    throw error(type, "sys_obj_get_type");

	int slot = (type_cache_next++) % type_cache_size;
	type_cache[slot].obj = dir.obj.object;
	type_cache[slot].type = type;
    }

    if (type == kobj_container) {
	try {
	    return new fs_dir_dseg_cached(dir, writable);
	} catch (missing_dir_segment &m) {
	    return new fs_dir_ct(dir);
	}
    } else if (type == kobj_mlt) {
	return new fs_dir_mlt(dir);
    } else {
	throw basic_exception("unknown directory type %d", type);
    }
}

int
fs_readdir_init(struct fs_readdir_state *s, struct fs_inode dir)
{
    try {
	s->fs_dir_obj = fs_dir_open(dir, 0);
	s->fs_dir_iterator_obj = new fs_dir_iterator();
	s->dir = dir;
    } catch (error &e) {
	cprintf("fs_readdir_init: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("fs_readdir_init: %s\n", e.what());
	return -E_INVAL;
    }

    return 0;
}

void
fs_readdir_close(struct fs_readdir_state *s)
{
    fs_dir *d = (fs_dir *) s->fs_dir_obj;
    fs_dir_iterator *i = (fs_dir_iterator *) s->fs_dir_iterator_obj;

    delete d;
    delete i;
}

int
fs_readdir_dent(struct fs_readdir_state *s, struct fs_dent *de,
		struct fs_readdir_pos *p)
{
    fs_dir *d = (fs_dir *) s->fs_dir_obj;
    fs_dir_iterator *i = (fs_dir_iterator *) s->fs_dir_iterator_obj;

    if (p == 0)
	p = i;

    // For the debugging cprintf further down
    de->de_name[0] = '\0';

    int r;
    try {
	r = d->list(p, de);
    } catch (error &e) {
	cprintf("fs_readdir_dent: %s\n", e.what());
	r = e.err();
    } catch (std::exception &e) {
	cprintf("fs_readdir_dent: %s\n", e.what());
	r = -E_INVAL;
    }

    if (fs_debug)
	cprintf("fs_get_dent: dir %ld r %d obj %ld name %s\n",
		s->dir.obj.object, r, de->de_inode.obj.object,
		&de->de_name[0]);

    return r;
}

static int
fs_lookup_one(struct fs_inode dir, const char *fn, struct fs_inode *o)
{
    if (fs_debug)
	cprintf("fs_lookup_one: dir %ld fn %s\n",
		dir.obj.object, fn);

    if (!strcmp(fn, ".")) {
	*o = dir;
	return 0;
    }

    for (int i = 0; i < FS_NMOUNT; i++) {
	struct fs_mtab_ent *mnt = &fs_mtab->mtab_ent[i];
	if (mnt->mnt_dir.obj.object == dir.obj.object &&
	    !strcmp(&mnt->mnt_name[0], fn))
	{
	    *o = mnt->mnt_root;
	    return 0;
	}
    }

#if 0
    // Simple MLT support
    if (!strcmp(fn, "@mlt")) {
	uint8_t blob[MLT_BUF_SIZE];
	uint64_t ct;
	int retry_count = 0;

	struct ulabel *lcur = label_get_current();
	if (lcur == 0)
	    return -E_NO_MEM;
	scope_guard<void, struct ulabel *> lf1(label_free, lcur);

	struct ulabel *lslot = label_alloc();
	if (lslot == 0)
	    return -E_NO_MEM;
	scope_guard<void, struct ulabel *> lf2(label_free, lslot);

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

	    r = sys_mlt_put(dir.obj, lcur, &blob[0], &ct);
	    if (r < 0)
		return r;

	    o->obj = COBJ(ct, ct);
	    fs_dir_dseg::init(*o);
	}

	o->obj = COBJ(ct, ct);
	return 0;
    }
#endif

    try {
	fs_dir *d = fs_dir_open(dir, 0);
	scope_guard<void, fs_dir *> g(delete_obj, d);

	// Just get the first name
	fs_dir_iterator i;
	int found = d->lookup(fn, &i, o);
	if (found)
	    return 0;
	return -E_NOT_FOUND;
    } catch (error &e) {
	cprintf("fs_lookup_one: %s\n", e.what());
	return e.err();
    } catch (std::exception &e) {
	cprintf("fs_lookup_one: %s\n", e.what());
	return -E_INVAL;
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
fs_mkdir(struct fs_inode dir, const char *fn, struct fs_inode *o, struct ulabel *l)
{
    uint64_t avoid_types = ~0UL;
    avoid_types &= ~(1 << kobj_segment);
    avoid_types &= ~(1 << kobj_container);

    int64_t id = sys_container_alloc(dir.obj.object, l, fn, avoid_types, CT_QUOTA_INF);
    if (id < 0)
	return id;

    o->obj = COBJ(dir.obj.object, id);
    try {
	fs_dir_dseg::init(*o);
    } catch (error &e) {
	cprintf("fs_mkdir: %s\n", e.what());
	return e.err();
    }

    try {
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);
	d->insert(fn, *o);
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
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);
	d->insert(fn, *o);
    } catch (error &e) {
	sys_obj_unref(o->obj);
	return e.err();
    }

    return 0;
}

int
fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f, struct ulabel *l)
{
    int64_t id = sys_segment_create(dir.obj.object, 0, l, fn);
    if (id < 0) {
	if (id == -E_LABEL && start_env->declassify_gate.object) {
	    struct gate_call_data gcd;
	    struct declassify_args *darg =
		(struct declassify_args *) &gcd.param_buf[0];
	    darg->req = declassify_fs_create;
	    darg->fs_create.dir = dir;
	    snprintf(&darg->fs_create.name[0],
		     sizeof(darg->fs_create.name),
		     "%s", fn);

	    label verify;
	    thread_cur_label(&verify);
	    gate_call(start_env->declassify_gate, 0, 0, 0).call(&gcd, &verify);
	    *f = darg->fs_create.new_file;
	    return darg->status;
	}

	return id;
    }

    f->obj = COBJ(dir.obj.object, id);

    try {
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);
	d->insert(fn, *f);
    } catch (error &e) {
	sys_obj_unref(f->obj);
	return e.err();
    }

    return 0;
}

int
fs_link(struct fs_inode dir, const char *fn, struct fs_inode f)
{
    int r = sys_segment_addref(f.obj, dir.obj.object);
    if (r < 0)
	return r;

    struct fs_inode nf;
    nf.obj = COBJ(dir.obj.object, f.obj.object);

    try {
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);
	d->insert(fn, nf);
    } catch (error &e) {
	sys_obj_unref(nf.obj);
	return e.err();
    }

    return 0;
}

int
fs_remove(struct fs_inode dir, const char *name, struct fs_inode f)
{
    try {
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);

	int r = sys_obj_unref(f.obj);
	if (r < 0)
	    return r;

	d->remove(name, f);
    } catch (error &e) {
	cprintf("fs_remove: %s\n", e.what());
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

	// Corner case: if pn is "/foo", then dirname="/" and basename="foo"
	if (**dirname == '\0')
	    *dirname = "/";
    }
}
