#ifndef JOS_INC_LFS_H
#define JOS_INC_LFS_H

#include <inc/container.h>

struct lfs_descriptor {
    struct cobj_ref gate;
    char pn[128];
};

#endif
