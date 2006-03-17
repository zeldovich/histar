#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>

#include <bits/unimpl.h>

int
__libc_open(const char *pn, int flags, ...) __THROW
{
    if (!strcmp(pn, "")) {
	__set_errno(ENOENT);
	return -1;
    }

    int just_created = 0;
    struct fs_inode ino;
    int r = fs_namei(pn, &ino);
    if (r == 0) {
	if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
	    __set_errno(EEXIST);
	    return -1;
	}
    } else if (r == -E_NOT_FOUND) {
	if (!(flags & O_CREAT)) {
	    __set_errno(ENOENT);
	    return -1;
	}

	char *pn2 = malloc(strlen(pn) + 1);
	if (pn2 == 0) {
	    __set_errno(ENOMEM);
	    return -1;
	}

	strcpy(pn2, pn);
	const char *dirname, *basename;
	fs_dirbase(pn2, &dirname, &basename);

	struct fs_inode dir;
	r = fs_namei(dirname, &dir);
	if (r < 0) {
	    free(pn2);
	    __set_errno(ENOENT);
	    return -1;
	}

	r = fs_create(dir, basename, &ino);
	free(pn2);
	if (r < 0) {
	    __set_errno(EPERM);
	    return -1;
	}

	just_created = 1;
    } else {
	__set_errno(EPERM);
	return -1;
    }

    if ((flags & O_TRUNC) && !just_created) {
	r = fs_resize(ino, 0);
	if (r < 0) {
	    __set_errno(EPERM);
	    return -1;
	}
    }

    struct Fd *fd;
    r = fd_alloc(&fd, "file fd");
    if (r < 0) {
	__set_errno(ENOMEM);
	return -1;
    }

    fd->fd_dev_id = devfile.dev_id;
    fd->fd_omode = flags;
    fd->fd_file.ino = ino;

    if ((flags & O_APPEND))
	fs_getsize(ino, &fd->fd_offset);

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
    int type = sys_obj_get_type(fd->fd_file.ino.obj);
    if (type < 0) {
	cprintf("file_stat: get_type: %s\n", e2s(type));
	__set_errno(EIO);
	return -1;
    }

    memset(buf, 0, sizeof(*buf));
    buf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
    if (type == kobj_container || type == kobj_mlt) {
	buf->st_mode |= __S_IFDIR;
    } else {
	buf->st_mode |= __S_IFREG;

	uint64_t len;
	int r = fs_getsize(fd->fd_file.ino, &len);
	if (r < 0) {
	    cprintf("file_stat: getsize: %s\n", e2s(r));
	    __set_errno(EIO);
	    return -1;
	}
	buf->st_size = len;
    }

    return 0;
}

static ssize_t
file_getdents(struct Fd *fd, struct dirent *buf, size_t nbytes)
{
    struct fs_readdir_state s;

    int r = fs_readdir_init(&s, fd->fd_file.ino);
    if (r < 0) {
	__set_errno(ENOTDIR);
	return -1;
    }

    size_t dirent_base = offsetof (struct dirent, d_name);

    size_t cc = 0;
    for (;;) {
	if (cc >= nbytes)
	    break;

	struct fs_readdir_pos savepos = fd->fd_file.readdir_pos;
	struct fs_dent de;
	r = fs_readdir_dent(&s, &de, &fd->fd_file.readdir_pos);
	if (r <= 0) {
	    fs_readdir_close(&s);
	    fd->fd_file.readdir_pos = savepos;

	    if (cc > 0)
		return cc;
	    if (r == 0)
		return 0;

	    __set_errno(EIO);
	    return -1;
	}

	size_t space = nbytes - cc;
	size_t namlen = strlen(&de.de_name[0]);
	size_t reclen = dirent_base + namlen + 1;
	if (space < reclen) {
	    fd->fd_file.readdir_pos = savepos;
	    break;
	}

	buf->d_ino = de.de_inode.obj.object;
	buf->d_off = buf->d_ino;
	buf->d_reclen = reclen;
	buf->d_type = DT_UNKNOWN;
	memcpy(&buf->d_name[0], &de.de_name[0], namlen + 1);
	cc += reclen;

	buf = (struct dirent *) (((char *) buf) + reclen);
    }

    fs_readdir_close(&s);

    if (cc == 0) {
	__set_errno(EINVAL);
	return -1;
    }

    return cc;
}

static int
file_probe(struct Fd *fd, dev_probe_t probe)
{
    return 1 ;
}

struct Dev devfile = {
    .dev_id = 'f',
    .dev_name = "file",
    .dev_read = file_read,
    .dev_write = file_write,
    .dev_close = file_close,
    .dev_stat = file_stat,
    .dev_getdents = file_getdents,
    .dev_probe = file_probe,
};

weak_alias(__libc_open, open);
weak_alias(__libc_open, open64);

