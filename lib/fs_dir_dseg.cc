extern "C" {
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/lib.h>
#include <string.h>
#include <inc/error.h>
#include <inc/memlayout.h>
}

#include <inc/fs_dir.hh>
#include <inc/error.hh>
#include <inc/scopeguard.hh>
#include <inc/cpplabel.hh>
#include <inc/labelutil.hh>
#include <inc/pthread.hh>

struct fs_dirslot {
    uint64_t id;
    uint8_t inuse;
    char name[KOBJ_NAME_LEN];
};

struct fs_directory {
    pthread_mutex_t lock;
    struct fs_dirslot slots[0];
};

fs_dir_dseg::fs_dir_dseg(fs_inode dir, bool writable)
    : writable_(writable), ino_(dir)
{
    int64_t r = sys_container_get_slot_id(ino_.obj.object, 0);
    if (r < 0)
	throw missing_dir_segment(r, "sys_container_get_slot_id");

    dseg_ = COBJ(ino_.obj.object, r);

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
    error_check(segment_map(dseg_, perm, (void **) &dir_, &dirsize));

    dir_end_ = ((char *) dir_) + dirsize;

    if (writable_)
	pthread_mutex_lock(&dir_->lock);
}

fs_dir_dseg::~fs_dir_dseg()
{
    if (writable_)
	pthread_mutex_unlock(&dir_->lock);
    segment_unmap(dir_);
}

void
fs_dir_dseg::remove(const char *name, fs_inode ino)
{
    check_writable();
    fs_dirslot *slot;

    for (slot = &dir_->slots[0]; (slot + 1) <= (fs_dirslot *) dir_end_; slot++)
	if (slot->inuse && slot->id == ino.obj.object && !strcmp(slot->name, name))
	    slot->inuse = 0;
}

int
fs_dir_dseg::lookup(const char *name, fs_readdir_pos *i, fs_inode *ino)
{
    for (;;) {
	fs_dirslot *slot = &dir_->slots[i->a++];
	if ((slot + 1) > (fs_dirslot *) dir_end_)
	    return 0;

	if (slot->inuse && !strcmp(slot->name, name)) {
	    ino->obj = COBJ(ino_.obj.object, slot->id);
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
	struct fs_dirslot *slot;
	for (slot = &dir_->slots[0];
	    (slot + 1) <= (struct fs_dirslot *) dir_end_;
	    slot++)
	{
	    if (!slot->inuse) {
		strncpy(&slot->name[0], name, KOBJ_NAME_LEN - 1);
		slot->id = ino.obj.object;
		slot->inuse = 1;
		return;
	    }
	}

	grow();
    }
}

int
fs_dir_dseg::list(fs_readdir_pos *i, fs_dent *de)
{
retry:
    struct fs_dirslot *slot = &dir_->slots[i->a++];
    if (slot + 1 > (struct fs_dirslot *) dir_end_)
	return 0;
    if (slot->inuse == 0)
	goto retry;

    memcpy(&de->de_name[0], &slot->name[0], KOBJ_NAME_LEN);
    de->de_inode.obj = COBJ(ino_.obj.object, slot->id);
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

    int64_t curbytes = sys_segment_get_nbytes(dseg_);
    if (curbytes < 0)
	throw error(curbytes, "sys_segment_get_nbytes");

    error_check(segment_unmap(dir_));
    error_check(sys_segment_resize(dseg_, curbytes + PGSIZE));

    uint64_t dirsize;
    error_check(segment_map(dseg_, SEGMAP_READ | SEGMAP_WRITE,
			    (void **) &dir_, &dirsize));

    dir_end_ = ((char *) dir_) + dirsize;
}

void
fs_dir_dseg::init(fs_inode dir)
{
    label l;
    obj_get_label(dir.obj, &l);

    struct cobj_ref dseg;
    error_check(segment_alloc(dir.obj.object, PGSIZE, &dseg,
			      0, l.to_ulabel(), "directory segment"));
}
