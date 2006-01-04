#ifndef _JOS_INC_FS_H
#define _JOS_INC_FS_H

/*
 * A really crude filesystem.
 */

#include <inc/container.h>

struct fs_dent {
    char de_name[64];
    struct cobj_ref de_cobj;
};

int  fs_get_root(uint64_t rc, struct cobj_ref *o);
int  fs_get_dent(struct cobj_ref d, int n, struct fs_dent *e);
int  fs_lookup(struct cobj_ref d, const char *pn, struct cobj_ref *o);

#endif
