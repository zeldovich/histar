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

int  fs_get_root(uint64_t container, struct fs_inode *rdirp);
int  fs_get_dent(struct fs_inode dir, uint64_t n, struct fs_dent *e);
int  fs_get_obj(struct fs_inode ino, struct cobj_ref *segp);
int  fs_lookup_one(struct fs_inode dir, const char *fn, struct fs_inode *o);
int  fs_lookup_path(struct fs_inode root, const char *pn, struct fs_inode *o);
int  fs_namei(const char *pn, struct fs_inode *o);

#endif
