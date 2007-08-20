#ifndef JOS_INC_FS_DIR_HH
#define JOS_INC_FS_DIR_HH

extern "C" {
#include <inc/fs.h>
}

#include <inc/error.hh>

class fs_dir_iterator : public fs_readdir_pos {
 public:
    fs_dir_iterator() { a = 0; dot_pos = 0; mtab_pos = 0; }
};

// A directory is simply a mapping of names to inodes.
// Provides a basic lookup() interface based on list().
class fs_dir {
 public:
    virtual ~fs_dir() {}

    virtual void insert(const char *name, fs_inode ino) = 0;
    virtual void remove(const char *name, fs_inode ino) = 0;

    // Returns 1 if found, 0 if not found
    virtual int lookup(const char *name, fs_readdir_pos *i, fs_inode *ino);

    // Returns 1 on valid entry, 0 for EOF
    virtual int list(fs_readdir_pos *i, fs_dent *de) = 0;
};

// Basic container directory:
// - list() enumerates container
// - insert() and remove() check that this is the right container
class fs_dir_ct : public fs_dir {
 public:
    fs_dir_ct(fs_inode dir) : ino_(dir) {}

    virtual void insert(const char *name, fs_inode ino);
    virtual void remove(const char *name, fs_inode ino);
    virtual int list(fs_readdir_pos *i, fs_dent *de);

 private:
    fs_dir_ct(const fs_dir_ct&);
    fs_dir_ct &operator=(const fs_dir_ct&);

    fs_inode ino_;
};

// Segment-based directory
class missing_dir_segment : public error {
 public:
    missing_dir_segment(int r, const char *m) : error(r, "%s", m) {}
};

class fs_dir_dseg : public fs_dir {
 public:
    fs_dir_dseg(fs_inode dir, bool writable);
    virtual ~fs_dir_dseg();

    virtual void insert(const char *name, fs_inode ino);
    virtual void remove(const char *name, fs_inode ino);
    virtual int lookup(const char *name, fs_readdir_pos *i, fs_inode *ino);
    virtual int list(fs_readdir_pos *i, fs_dent *de);

    void lock();
    void unlock();
    void refresh();

    static void init(fs_inode dir);

 private:
    fs_dir_dseg(const fs_dir_dseg&);
    fs_dir_dseg &operator=(const fs_dir_dseg&);

    void check_writable();
    void grow();

    bool writable_;
    bool locked_;
    fs_inode ino_;
    struct cobj_ref dseg_;
    struct fs_directory *dir_;
    void *dir_end_;
};

// Cached segment-based directory
class fs_dir_dseg_cached : public fs_dir {
 public:
    fs_dir_dseg_cached(fs_inode dir, bool writable);
    virtual ~fs_dir_dseg_cached();

    virtual void insert(const char *name, fs_inode ino)
	{ backer_->insert(name, ino); }
    virtual void remove(const char *name, fs_inode ino)
	{ backer_->remove(name, ino); }
    virtual int lookup(const char *name, fs_readdir_pos *i, fs_inode *ino)
	{ return backer_->lookup(name, i, ino); }
    virtual int list(fs_readdir_pos *i, fs_dent *de)
	{ return backer_->list(i, de); }

 private:
    fs_dir_dseg_cached(const fs_dir_dseg_cached&);
    fs_dir_dseg_cached &operator=(const fs_dir_dseg_cached&);

    fs_dir_dseg *backer_;
    int slot_;
};

#endif
