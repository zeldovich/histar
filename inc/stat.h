#ifndef JOS_INC_STAT_H
#define JOS_INC_STAT_H

struct stat;
struct stat64;

int jos_stat(struct fs_inode ino, struct stat64 *buf)
    __attribute__((warn_unused_result));
int jos_stat64_to_stat(struct stat64 *src, struct stat *dst)
    __attribute__((warn_unused_result));

#endif
