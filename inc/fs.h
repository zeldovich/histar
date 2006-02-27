#ifndef _JOS_INC_FS_H
#define _JOS_INC_FS_H

/*
 * A really crude filesystem.
 */

#include <inc/container.h>
#include <inc/kobj.h>

struct fs_inode {
    struct cobj_ref obj;
};

struct fs_dent {
    char de_name[KOBJ_NAME_LEN];
    struct fs_inode de_inode;
};

struct fs_mtab_ent {
    struct fs_inode mnt_dir;
    char mnt_name[KOBJ_NAME_LEN];
    struct fs_inode mnt_root;
};

#define FS_NMOUNT	16
struct fs_mount_table {
    struct fs_mtab_ent mtab_ent[FS_NMOUNT];
};

int  fs_get_root(uint64_t container, struct fs_inode *rdirp);
int  fs_get_dent(struct fs_inode dir, uint64_t n, struct fs_dent *e);
int  fs_get_obj(struct fs_inode ino, struct cobj_ref *segp);
int  fs_namei(const char *pn, struct fs_inode *o);

void fs_dirbase(char *pn, const char **dirname, const char **basename);

int  fs_mkdir(struct fs_inode dir, const char *fn, struct fs_inode *o);
int  fs_mkmlt(struct fs_inode dir, const char *fn, struct fs_inode *o);
int  fs_mount(struct fs_inode dir, const char *mnt_name, struct fs_inode root);
void fs_unmount(struct fs_inode dir, const char *mnt_name);
int  fs_create(struct fs_inode dir, const char *fn, struct fs_inode *f);
int  fs_remove(struct fs_inode f);
int  fs_link(struct fs_inode dir, const char *fn, struct fs_inode f);

int  fs_pwrite(struct fs_inode f, const void *buf, uint64_t count, uint64_t off);
int  fs_pread(struct fs_inode f, void *buf, uint64_t count, uint64_t off);
int  fs_getsize(struct fs_inode f, uint64_t *len);

#endif
