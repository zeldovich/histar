#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <bits/unimpl.h>

int
mkdir(const char *pn, mode_t mode)
{
    char *pn2 = malloc(strlen(pn) + 1);
    if (pn2 == 0)
	return -E_NO_MEM;

    strcpy(pn2, pn);
    const char *dirname, *basename;
    fs_dirbase(pn2, &dirname, &basename);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0) {
	free(pn2);
	return r;
    }

    struct fs_inode ndir;
    r = fs_mkdir(dir, basename, &ndir);
    free(pn2);
    return r;
}

int
unlink(const char *pn)
{
    char *pn2 = malloc(strlen(pn) + 1);
    if (pn2 == 0)
	return -E_NO_MEM;

    strcpy(pn2, pn);
    const char *dirname, *basename;
    fs_dirbase(pn2, &dirname, &basename);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0) {
	free(pn2);
	return r;
    }

    struct fs_inode f;
    r = fs_namei(pn, &f);
    if (r < 0) {
	free(pn2);
	return r;
    }

    r = fs_remove(dir, basename, f);
    free(pn2);
    return r;
}

int
rename(const char *src, const char *dst)
{
    set_enosys();
    return -1;
}

int
chdir(const char *pn)
{
    struct fs_inode dir;
    int r = fs_namei(pn, &dir);
    if (r < 0)
	return r;

    start_env->fs_cwd = dir;
    return 0;
}

mode_t
umask(mode_t mask)
{
    return 0;
}

int
stat(const char *file_name, struct stat *buf)
{
    int fd = open(file_name, O_RDONLY);
    if (fd < 0)
	return fd;

    int r = fstat(fd, buf);
    close(fd);

    return r;
}

int
lstat(const char *file_name, struct stat *buf)
{
    return stat(file_name, buf);
}

int
access(const char *pn, int mode)
{
    // XXX lie about it, for now..
    return 0;
}

int
readlink(const char *pn, char *buf, size_t bufsize)
{
    __set_errno(EINVAL);
    return -1;
}

char *
getcwd(char *buf, size_t size)
{
    if (buf == 0) {
	if (size == 0)
	    size = 256;
	buf = malloc(size);
	if (buf == 0)
	    return -1;
    }

    // XXX we do not have a ".." yet
    sprintf(buf, "/unknown");
    return buf;
}
