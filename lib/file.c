#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>

int
mkdir(const char *pn, int mode)
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
    struct fs_inode f;
    int r = fs_namei(pn, &f);
    if (r < 0)
	return r;

    return fs_remove(f);
}

int
open(const char *pn, int flags, int mode)
{
    int r;
    struct fs_inode ino;
    if ((flags | O_CREAT)) {
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
    r = fd_alloc(start_env->container, &fd, "file fd");
    if (r < 0)
	return r;

    fd->fd_dev_id = devfile.dev_id;
    fd->fd_omode = flags;
    fd->fd_file.ino = ino;

    return fd2num(fd);
}

static int
file_read(struct Fd *fd, void *buf, size_t n, off_t offset)
{
    return fs_pread(fd->fd_file.ino, buf, n, offset);
}

static int
file_write(struct Fd *fd, const void *buf, size_t n, off_t offset)
{
    return fs_pwrite(fd->fd_file.ino, buf, n, offset);
}

static int
file_close(struct Fd *fd)
{
    return 0;
}

struct Dev devfile = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
    .dev_close = file_close,
};
