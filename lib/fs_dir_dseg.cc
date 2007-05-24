extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <inc/stdio.h>
#include <inc/assert.h>

#include <string.h>
}

#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/jthread.hh>

#define DIR_GEN_BUSY	(~0UL)

struct fs_dirslot {
    uint64_t gen;
    uint64_t id;
    uint8_t inuse;
    char name[KOBJ_NAME_LEN];
};

struct fs_directory {
    jthread_mutex_t lock;
    uint64_t extra_pages;
    struct fs_dirslot slots[0];
};

fs_dir_dseg::fs_dir_dseg(fs_inode dir, bool writable)
    : writable_(writable), locked_(false), ino_(dir),
      dseg_(COBJ(0, 0)), dir_(0), dir_end_(0)
{
    struct fs_object_meta meta;
    error_check(sys_obj_get_meta(ino_.obj, &meta));
    
    if (!meta.dseg_id)
	throw missing_dir_segment(-E_NOT_FOUND, "segment id not in meta data");

    dseg_ = COBJ(ino_.obj.object, meta.dseg_id);

    dir_ = 0;
    uint64_t perm = SEGMAP_READ;
    if (writable_)
	perm |= SEGMAP_WRITE;

    uint64_t dirsize = 0;
    error_check(segment_map(dseg_, 0, perm, (void **) &dir_, &dirsize, 0));

    dir_end_ = ((char *) dir_) + dirsize;

    lock();
}

fs_dir_dseg::~fs_dir_dseg()
{
    unlock();
    segment_unmap(dir_);
}

void
fs_dir_dseg::remove(const char *name, fs_inode ino)
{
    check_writable();
    fs_dirslot *slot;

    for (slot = &dir_->slots[0]; (slot + 1) <= (fs_dirslot *) dir_end_; slot++) {
	if (slot->inuse && slot->id == ino.obj.object && !strcmp(slot->name, name)) {
	    uint64_t ngen = slot->gen + 1;
	    slot->gen = DIR_GEN_BUSY;
	    slot->inuse = 0;
	    slot->gen = ngen;
	}
    }
}

int
fs_dir_dseg::lookup(const char *name, fs_readdir_pos *i, fs_inode *ino)
{
    if (!(i->b++) && !strcmp(name, "..")) {
	int64_t parent_id;
	error_check((parent_id = sys_container_get_parent(ino_.obj.object)));
	ino->obj = COBJ(parent_id, parent_id);
	return 1;
    }

    for (;;) {
	volatile fs_dirslot *slot = &dir_->slots[i->a++];
	if ((slot + 1) > (fs_dirslot *) dir_end_)
	    return 0;

	uint64_t start_gen, end_gen;
	fs_dirslot copy;
	do {
	    start_gen = slot->gen;
	    memcpy(&copy, (const void *) slot, sizeof(copy));
	    end_gen = slot->gen;
	} while (start_gen == DIR_GEN_BUSY || start_gen != end_gen);

	if (copy.inuse && !strcmp(copy.name, name)) {
	    ino->obj = COBJ(ino_.obj.object, copy.id);
	    return 1;
	}
    }
}

void
fs_dir_dseg::insert(const char *name, fs_inode ino)
{
    check_writable();

    if (ino_.obj.object != ino.obj.container)
	throw basic_exception("fs_dir_dseg::insert: bad inode");

    for (;;) {
	volatile fs_dirslot *slot;
	for (slot = &dir_->slots[0];
	    (slot + 1) <= (struct fs_dirslot *) dir_end_;
	    slot++)
	{
	    if (!slot->inuse) {
		uint64_t ngen = slot->gen + 1;
		slot->gen = DIR_GEN_BUSY;
		strncpy((char *) &slot->name[0], name, KOBJ_NAME_LEN - 1);
		slot->id = ino.obj.object;
		slot->inuse = 1;
		slot->gen = ngen;
		return;
	    }
	}

	grow();
    }
}

int
fs_dir_dseg::list(fs_readdir_pos *i, fs_dent *de)
{
    int64_t parent_id;

    switch (i->b++) {
    case 0:
	sprintf(&de->de_name[0], ".");
	de->de_inode = ino_;
	return 1;

    case 1:
	sprintf(&de->de_name[0], "..");
	error_check((parent_id = sys_container_get_parent(ino_.obj.object)));
	de->de_inode.obj = COBJ(parent_id, parent_id);
	return 1;

    default:
	break;
    }

retry:
    volatile fs_dirslot *slot = &dir_->slots[i->a++];
    if (slot + 1 > (struct fs_dirslot *) dir_end_)
	return 0;

    uint64_t start_gen, end_gen;
    fs_dirslot copy;
    do {
	start_gen = slot->gen;
	memcpy(&copy, (const void *) slot, sizeof(copy));
	end_gen = slot->gen;
    } while (start_gen == DIR_GEN_BUSY || start_gen != end_gen);

    if (copy.inuse == 0)
	goto retry;

    memcpy(&de->de_name[0], &copy.name[0], KOBJ_NAME_LEN);
    de->de_inode.obj = COBJ(ino_.obj.object, copy.id);
    return 1;
}

void
fs_dir_dseg::check_writable()
{
    if (!writable_)
	throw basic_exception("fs_dir_dseg: not writable");
}

void
fs_dir_dseg::grow()
{
    check_writable();

    uint64_t pages = dir_->extra_pages + 1;
    error_check(sys_segment_resize(dseg_, (pages + 1) * PGSIZE));
    dir_->extra_pages = pages;

    refresh();
}

void
fs_dir_dseg::lock()
{
    if (writable_) {
	if (locked_)
	    panic("fs_dir_dseg::lock: already locked\n");

	jthread_mutex_lock(&dir_->lock);
	locked_ = true;
    }
}

void
fs_dir_dseg::unlock()
{
    if (writable_ && locked_) {
	jthread_mutex_unlock(&dir_->lock);
	locked_ = false;
    }
}

void
fs_dir_dseg::refresh()
{
    uint64_t mapped_bytes = (uint64_t) ((char *) dir_end_ - (char *) dir_);
    if (mapped_bytes != (dir_->extra_pages + 1) * PGSIZE) {
	error_check(segment_unmap(dir_));

	dir_ = 0;
	uint64_t dirsize = 0;
	error_check(segment_map(dseg_, 0, SEGMAP_READ | (writable_ ? SEGMAP_WRITE : 0),
				(void **) &dir_, &dirsize, 0));

	dir_end_ = ((char *) dir_) + dirsize;
    }
}

void
fs_dir_dseg::init(fs_inode dir)
{
    struct cobj_ref dseg;
    error_check(segment_alloc(dir.obj.object, PGSIZE, &dseg,
			      0, 0, "directory segment"));

    struct fs_object_meta meta;
    memset(&meta, 0, sizeof(meta));
    meta.mtime_nsec = meta.ctime_nsec = sys_clock_nsec();
    meta.dseg_id = dseg.object;
    error_check(sys_obj_set_meta(dir.obj, 0, &meta));
}

// Directory segment caching
enum { dseg_cache_size = 32 };

static struct dseg_cache_entry {
    fs_inode dir;
    bool writable;
    fs_dir_dseg *dseg;
    int ref;
} dseg_cache[dseg_cache_size];

static int dseg_cache_next;
static jthread_mutex_t dseg_cache_mu;

fs_dir_dseg_cached::fs_dir_dseg_cached(fs_inode dir, bool writable)
    : backer_(0), slot_(0)
{
    scoped_jthread_lock l(&dseg_cache_mu);

    for (int i = 0; i < dseg_cache_size; i++) {
	if (dir.obj.object == dseg_cache[i].dir.obj.object &&
	    writable == dseg_cache[i].writable)
	{
	    backer_ = dseg_cache[i].dseg;
	    assert(backer_);
	    backer_->lock();
	    backer_->refresh();
	    dseg_cache[i].ref++;
	    slot_ = i;
	    return;
	}
    }

    dseg_cache_next++;

    for (int i = 0; i < dseg_cache_size; i++) {
	slot_ = (dseg_cache_next + i) % dseg_cache_size;
	if (dseg_cache[slot_].ref)
	    continue;

	backer_ = new fs_dir_dseg(dir, writable);

	if (dseg_cache[slot_].dseg)
	    delete dseg_cache[slot_].dseg;

	dseg_cache[slot_].ref = 1;
	dseg_cache[slot_].dseg = backer_;
	dseg_cache[slot_].dir = dir;
	dseg_cache[slot_].writable = writable;
	return;
    }

    throw basic_exception("out of dseg cache slots");
}

fs_dir_dseg_cached::~fs_dir_dseg_cached()
{
    assert(backer_);
    backer_->unlock();
    dseg_cache[slot_].ref--;
}
