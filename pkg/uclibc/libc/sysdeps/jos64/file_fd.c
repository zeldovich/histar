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

int
__libc_open(const char *pn, int flags, ...) __THROW
{
    int r;
    struct fs_inode ino;
    if ((flags & O_CREAT)) {
	char *pn2 = malloc(strlen(pn) + 1);
	if (pn2 == 0)
	    return -E_NO_MEM;

	strcpy(pn2, pn);
	const char *dirname, *basename;
	fs_dirbase(pn2, &dirname, &basename);

	struct fs_inode dir;
	r = fs_namei(dirname, &dir);
	if (r < 0) {
	    free(pn2);
	    return r;
	}

	r = fs_create(dir, basename, &ino);
	free(pn2);
	if (r < 0)
	    return r;
    } else {
	r = fs_namei(pn, &ino);
	if (r < 0)
	    return r;
    }

    struct Fd *fd;
    r = fd_alloc(start_env->proc_container, &fd, "file fd");
    if (r < 0)
	return r;

    fd->fd_dev_id = devfile.dev_id;
    fd->fd_omode = flags;
    fd->fd_file.ino = ino;

    return fd2num(fd);
}

static ssize_t
file_read(struct Fd *fd, void *buf, size_t n, off_t offset)
{
    uint64_t flen;
    int r = fs_getsize(fd->fd_file.ino, &flen);
    if (r < 0) {
	cprintf("file_read: fs_getsize: %s\n", e2s(r));
	__set_errno(EIO);
	return -1;
    }

    if (offset > flen)
	n = 0;
    else
	n = MIN(n, flen - offset);

    r = fs_pread(fd->fd_file.ino, buf, n, offset);
    if (r < 0) {
	cprintf("file_read: fs_pread: %s\n", e2s(r));
	__set_errno(EIO);
	return -1;
    }

    return n;
}

static ssize_t
file_write(struct Fd *fd, const void *buf, size_t n, off_t offset)
{
    int r = fs_pwrite(fd->fd_file.ino, buf, n, offset);
    if (r < 0) {
	cprintf("file_write: %s\n", e2s(r));
	__set_errno(EIO);
	return -1;
    }

    return n;
}

static int
file_close(struct Fd *fd)
{
    return 0;
}

static int
file_stat(struct Fd *fd, struct stat *buf)
{
    uint64_t len;
    int r = fs_getsize(fd->fd_file.ino, &len);
    if (r < 0) {
	cprintf("file_stat: %s\n", e2s(r));
	__set_errno(EIO);
	return -1;
    }

    memset(buf, 0, sizeof(*buf));
    buf->st_size = len;

    return 0;
}

struct Dev devfile = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
    .dev_close = file_close,
    .dev_stat = file_stat,
};

weak_alias(__libc_open, open);
weak_alias(__libc_open, open64);

