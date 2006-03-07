#ifndef JOS_INC_FS_DIR_HH
#define JOS_INC_FS_DIR_HH

extern "C" {
#include <inc/fs.h>
}

#include <inc/error.hh>

class fs_dir_iterator {
public:
    fs_dir_iterator() : a(0), b(0) {}
    uint64_t a, b;
};

// A directory is simply a mapping of names to inodes.
// Provides a basic lookup() interface based on list().
class fs_dir {
public:
    virtual ~fs_dir() {}

    virtual void insert(const char *name, fs_inode ino) = 0;
    virtual void remove(const char *name, fs_inode ino) = 0;

    // Returns 1 if found, 0 if not found
    virtual int lookup(const char *name, fs_dir_iterator *i, fs_inode *ino);

    // Returns 1 on valid entry, 0 for EOF
    virtual int list(fs_dir_iterator *i, fs_dent *de) = 0;
};

// Basic container directory:
// - list() enumerates container
// - insert() and remove() check that this is the right container
class fs_dir_ct : public fs_dir {
public:
    fs_dir_ct(fs_inode dir) : ino_(dir) {}

    virtual void insert(const char *name, fs_inode ino);
    virtual void remove(const char *name, fs_inode ino);
    virtual int list(fs_dir_iterator *i, fs_dent *de);

private:
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
    virtual int lookup(const char *name, fs_dir_iterator *i, fs_inode *ino);
    virtual int list(fs_dir_iterator *i, fs_dent *de);

    static void init(fs_inode dir);

private:
    void check_writable();
    void grow();

    bool writable_;
    fs_inode ino_;
    struct cobj_ref dseg_;
    struct fs_directory *dir_;
    void *dir_end_;
};

// Non-unionizing MLT support
class fs_dir_mlt : public fs_dir {
public:
    fs_dir_mlt(fs_inode dir) : ino_(dir) {}

    virtual void __attribute__((noreturn)) insert(const char *name, fs_inode ino) {
	throw error(-E_BAD_OP, "fs_dir_mlt::insert");
    }

    virtual void __attribute__((noreturn)) remove(const char *name, fs_inode ino) {
	throw error(-E_BAD_OP, "fs_dir_mlt::remove");
    }

    virtual int list(fs_dir_iterator *i, fs_dent *de);

private:
    fs_inode ino_;
};

#endif
