#ifndef _JOS_INC_FS_H
#define _JOS_INC_FS_H

/*
 * A really crude filesystem.
 */

#include <inc/container.h>
#include <inc/kobj.h>
#include <inc/label.h>

#define FS_NAME_LEN	128

struct fs_inode {
    struct cobj_ref obj;
};

struct fs_dent {
    char de_name[FS_NAME_LEN];
    struct fs_inode de_inode;
};

struct fs_mtab_ent {
    struct fs_inode mnt_dir;
    char mnt_name[FS_NAME_LEN];
    struct fs_inode mnt_root;
};

#define FS_NMOUNT	16
struct fs_mount_table {
    struct fs_mtab_ent mtab_ent[FS_NMOUNT];
};

struct fs_readdir_pos {
    uint64_t a;
    uint64_t dot_pos;
    uint64_t mtab_pos;
};

struct fs_readdir_state {
    struct fs_inode dir;
    void *fs_dir_obj;
    void *fs_dir_iterator_obj;
};

// Metadata stored for filesystem objects.  Should be KOBJ_META_LEN in size.
struct fs_object_meta {
    uint64_t mtime_nsec;
    uint64_t ctime_nsec;
    uint64_t f_type;
    uint64_t dseg_id;		// directory containers

    uint8_t dev_id;
    uint8_t pad0[3];
    uint32_t dev_opt;

    uint64_t pad1[3];
};

#define NAMEI_LEAF_NOFOLLOW	0x01
#define NAMEI_LEAF_NOEVAL	0x02

void fs_get_root(uint64_t container, struct fs_inode *rdirp);
int  fs_namei(const char *pn, struct fs_inode *o);
int  fs_namei_flags(const char *pn, struct fs_inode *o, uint32_t flags);

int  fs_readdir_init(struct fs_readdir_state *s, struct fs_inode dir);
int  fs_readdir_dent(struct fs_readdir_state *s, struct fs_dent *de,
		     struct fs_readdir_pos *p);
void fs_readdir_close(struct fs_readdir_state *s);

void fs_dirbase(char *pn, const char **dirname, const char **basenam);

int  fs_mkdir(struct fs_inode dir, const char *fn, struct fs_inode *o, struct ulabel *l);
int  fs_mkmlt(struct fs_inode dir, const char *fn, struct fs_inode *o);
int  fs_mount(struct cobj_ref fs_mtab_seg, struct fs_inode dir, const char *mnt_name, struct fs_inode root);
void fs_unmount(struct cobj_ref fs_mtab_seg, struct fs_inode dir, const char *mnt_name);
int  fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f, struct ulabel *l);
int fs_move(struct fs_inode srcdir, struct fs_inode dstdir, struct fs_inode srcino, const char * old_fn, const char *new_fn);
int  fs_remove(struct fs_inode dir, const char *fn, struct fs_inode f);
int  fs_link(struct fs_inode dir, const char *fn, struct fs_inode f, int remove_old);
int  fs_mknod(struct fs_inode dir, const char *fn, uint32_t dev_id, uint32_t dev_opt, struct fs_inode *ino, struct ulabel *l);

int64_t fs_pwrite(struct fs_inode f, const void *buf, uint64_t count, uint64_t off);
int64_t fs_pread(struct fs_inode f, void *buf, uint64_t count, uint64_t off);
int  fs_resize(struct fs_inode f, uint64_t len);
int  fs_getsize(struct fs_inode f, uint64_t *len);

int  fs_taint_self(struct fs_inode f);
int  fs_clone_mtab(uint64_t ct);

#endif
