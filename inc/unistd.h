#ifndef JOS_INC_UNISTD_H
#define JOS_INC_UNISTD_H

#include <sys/types.h>
#include <fd.h>

int  unlink(const char *path);
int  chdir(const char *path);

#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
int  access(const char *path, int mode);

int  pipe(int fd[2]);

int  isatty(int fd);

gid_t getegid(void);

int  execve(const char *filename, char *const argv [], char *const envp[]);

#endif
