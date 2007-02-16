#ifndef JOS_INC_STAT_H
#define JOS_INC_WSTAT_H

struct stat;

int jos_stat(struct fs_inode ino, struct stat *buf);

#endif
