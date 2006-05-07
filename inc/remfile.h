#ifndef REMFILE_H_
#define REMFILE_H_

struct file_stat;

struct rem_inode 
{
    struct cobj_ref seg;
};

int remfiled_open(char *host, int port, char *path, struct rem_inode *ino);
ssize_t remfiled_read(struct rem_inode f, void *buf, uint64_t count, uint64_t off);
ssize_t remfiled_write(struct rem_inode f, const void *buf, uint64_t count, uint64_t off);
int remfiled_stat(struct rem_inode f, struct file_stat * buf);


#endif /*REMFILE_H_*/
