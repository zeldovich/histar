#ifndef JOS_INC_SYS_STAT_H
#define JOS_INC_SYS_STAT_H

#include <types.h>

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    time_t st_mtime;
    off_t st_size;
};

int stat(const char *file_name, struct stat *buf);
int lstat(const char *file_name, struct stat *buf);
int fstat(int fd, struct stat *buf);
mode_t umask(mode_t mask);

#define __S_IFMT        0170000 /* These bits determine file type.  */

#define __S_IFIFO       0010000 /* FIFO.  */
#define __S_IFCHR       0020000 /* Character device.  */
#define __S_IFDIR       0040000 /* Directory.  */
#define __S_IFBLK       0060000 /* Block device.  */
#define __S_IFREG       0100000 /* Regular file.  */
#define __S_IFLNK       0120000 /* Symbolic link.  */
#define __S_IFSOCK      0140000 /* Socket.  */

#define S_ISUID         04000
#define S_ISGID         02000
#define S_ISVTX         01000

#define S_IRUSR         0400
#define S_IWUSR         0200
#define S_IXUSR         0100
/* Read, write, and execute by owner.  */
#define S_IRWXU (S_IRUSR|S_IWUSR|S_IXUSR)

#define S_IRGRP (S_IRUSR >> 3)  /* Read by group.  */
#define S_IWGRP (S_IWUSR >> 3)  /* Write by group.  */
#define S_IXGRP (S_IXUSR >> 3)  /* Execute by group.  */
/* Read, write, and execute by group.  */
#define S_IRWXG (S_IRWXU >> 3)

#define S_IROTH (S_IRGRP >> 3)  /* Read by others.  */
#define S_IWOTH (S_IWGRP >> 3)  /* Write by others.  */
#define S_IXOTH (S_IXGRP >> 3)  /* Execute by others.  */
/* Read, write, and execute by others.  */
#define S_IRWXO (S_IRWXG >> 3)

#define __S_ISTYPE(mode, mask)  (((mode) & __S_IFMT) == (mask))
#define S_ISDIR(mode)    __S_ISTYPE((mode), __S_IFDIR)
#define S_ISCHR(mode)    __S_ISTYPE((mode), __S_IFCHR)
#define S_ISBLK(mode)    __S_ISTYPE((mode), __S_IFBLK)
#define S_ISREG(mode)    __S_ISTYPE((mode), __S_IFREG)
#define S_ISFIFO(mode)   __S_ISTYPE((mode), __S_IFIFO)
#define S_ISLNK(mode)    __S_ISTYPE((mode), __S_IFLNK)
#define S_ISSOCK(mode)   __S_ISTYPE((mode), __S_IFSOCK)

#endif
