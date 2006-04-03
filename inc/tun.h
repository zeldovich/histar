#ifndef JOS_INC_TUN_H
#define JOS_INC_TUN_H

#include <inc/fd.h>

int  jos_tun_open(struct fs_inode tseg, const char *pn_suffix, int flags);

#endif
