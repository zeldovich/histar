#ifndef REMFILE_H_
#define REMFILE_H_

struct rem_inode 
{
    struct cobj_ref seg;
};

int remfiled_open(char *host, int port, char *path, struct rem_inode *ino);
ssize_t remfiled_read(struct rem_inode f, void *buf, uint64_t count, uint64_t off);
ssize_t remfiled_write(struct rem_inode f, const void *buf, uint64_t count, uint64_t off);

#endif /*REMFILE_H_*/
