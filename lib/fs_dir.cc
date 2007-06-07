extern "C" {
#include <inc/fs.h>
#include <inc/fd.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/declassify.h>
#include <inc/gateparam.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/time.h>

#include <string.h>
#include <inttypes.h>
}

#include <inc/gateclnt.hh>
#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/labelutil.hh>
#include <inc/scopedprof.hh>

static int fs_debug = 0;

enum { type_cache_size = 32 };
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
	cprintf("fs_get_dent: dir %"PRIu64" r %d obj %"PRIu64" name %s\n",
		s->dir.obj.object, r, de->de_inode.obj.object,
		&de->de_name[0]);

    return r;
}

static int
fs_lookup_one(struct fs_inode dir, const char *fn, struct fs_inode *o, 
	      struct fs_mount_table *mtab)
{
    if (fs_debug)
	cprintf("fs_lookup_one: dir %"PRIu64" fn %s\n",
		dir.obj.object, fn);

    if (!strcmp(fn, ".")) {
	*o = dir;
	return 0;
    }

    if (mtab) {
	for (int i = 0; i < FS_NMOUNT; i++) {
	    struct fs_mtab_ent *mnt = &mtab->mtab_ent[i];
	    if (mnt->mnt_dir.obj.object == dir.obj.object &&
		!strcmp(&mnt->mnt_name[0], fn))
	    {
		*o = mnt->mnt_root;
		return 0;
	    }
	}
    }

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
fs_lookup_path(struct fs_inode start_dir, const char *pn,
	       struct fs_inode *o, int symlinks,
	       uint32_t flags)
{
    if (pn[0] == '#') {
	uint64_t ctid;
	int r = strtou64(pn + 1, 0, 10, &ctid);
	if (r == 0) {
	    o->obj = COBJ(ctid, ctid);
	    return 0;
	}
    }

    if (pn[0] == '/')
	start_dir = start_env->fs_root;

    *o = start_dir;

    struct fs_mount_table *mtab = 0;
    uint64_t mtlen = sizeof(*mtab);
    int r = segment_map(start_env->fs_mtab_seg, 0, SEGMAP_READ,
			(void **) &mtab, &mtlen, 0);
    scope_guard2<int, void*, int> unmap(segment_unmap_delayed, mtab, 1);
    if (r < 0) {
	unmap.dismiss();
	mtab = 0;
    }

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
	    fs_inode cur_dir = *o;

	    r = fs_lookup_one(cur_dir, &fn[0], o, mtab);
	    if (r < 0)
		return r;

	    /* Handle symlinks */
	    if (!next_pn[0] && (flags & NAMEI_LEAF_NOEVAL))
		break;

	    struct fs_object_meta m;
	    r = sys_obj_get_meta(o->obj, &m);
	    if (r < 0)
		return r;

	    if (m.dev_id == devsymlink.dev_id) {
		if (!symlinks || (!next_pn[0] && (flags & NAMEI_LEAF_NOFOLLOW)))
		    return -E_INVAL;

		char linkbuf[1024];
		ssize_t cc = fs_pread(*o, &linkbuf[0], sizeof(linkbuf), 0);
		if (cc < 0)
		    return cc;

		linkbuf[cc] = '\0';
		symlinks--;
		r = fs_lookup_path(cur_dir, linkbuf, o, symlinks, 0);
		if (r < 0)
		    return r;
	    }
	}

	pn = next_pn;
    }

    return 0;
}

int
fs_namei(const char *pn, struct fs_inode *o)
{
    return fs_lookup_path(start_env->fs_cwd, pn, o, MAXSYMLINKS, 0);
}

int
fs_namei_flags(const char *pn, struct fs_inode *o, uint32_t flags)
{
    return fs_lookup_path(start_env->fs_cwd, pn, o, MAXSYMLINKS, flags);
}

int
fs_mkdir(struct fs_inode dir, const char *fn, struct fs_inode *o, struct ulabel *l)
{
    uint64_t avoid_types = UINT64(~0);
    avoid_types &= ~(1 << kobj_segment);
    avoid_types &= ~(1 << kobj_container);

    int64_t id = sys_container_alloc(dir.obj.object, l, fn, avoid_types, CT_QUOTA_INF);
    if (id < 0)
	return id;

    o->obj = COBJ(dir.obj.object, id);
    scope_guard<int, cobj_ref> unref(sys_obj_unref, o->obj);
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
	return e.err();
    }
    
    unref.dismiss();
    return 0;
}

int
fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f, struct ulabel *l)
{
    return fs_mknod(dir, fn, devfile.dev_id, 0, f, l);
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

int
fs_rename(struct fs_inode dir, const char *oldfn, const char *newfn, struct fs_inode f)
{
    try {
	fs_dir *d = fs_dir_open(dir, 1);
	scope_guard<void, fs_dir *> g(delete_obj, d);

	d->insert(newfn, f);
	d->remove(oldfn, f);
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

	// Corner case: if pn is "/foo", then dirname="/" and basename="foo"
	if (**dirname == '\0')
	    *dirname = "/";
    }
}

int  
fs_mknod(struct fs_inode dir, const char *fn, uint32_t dev_id, uint32_t dev_opt,
	 struct fs_inode *f, struct ulabel *l)
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

    struct fs_object_meta meta;
    meta.mtime_nsec = meta.ctime_nsec = jos_time_nsec();
    meta.dev_id = dev_id;
    meta.dev_opt = dev_opt;
    int r = sys_obj_set_meta(f->obj, 0, &meta);
    if (r < 0) {
	sys_obj_unref(f->obj);
	return r;
    }

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
