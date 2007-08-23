#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/tun.h>
#include <inc/chardevs.h>
#include <inc/syscall.h>
#include <inc/stat.h>
#include <inc/time.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <bits/unimpl.h>

// Fake prototype to make GCC happy
int __libc_open(const char *pn, int flags, ...);

int
__libc_open(const char *pn, int flags, ...)
{
    if (!strcmp(pn, "")) {
	__set_errno(ENOENT);
	return -1;
    }

    int just_created = 0;
    struct fs_inode ino;
    int r = fs_namei_flags(pn, &ino, (flags & O_NOFOLLOW) ? NAMEI_LEAF_NOFOLLOW : 0);
    if (r == 0) {
	if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
	    __set_errno(EEXIST);
	    return -1;
	}

	struct fs_object_meta m;
	r = sys_obj_get_meta(ino.obj, &m);
	struct Dev *dev;
	if (r >= 0 && (dev_lookup(m.dev_id, &dev) >= 0) && dev->dev_open)
	    return dev->dev_open(ino, flags, m.dev_opt);	
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
	const char *dirname, *basenm;
	fs_dirbase(pn2, &dirname, &basenm);

	struct fs_inode dir;
	r = fs_namei(dirname, &dir);
	if (r < 0) {
	    free(pn2);
	    __set_errno(ENOENT);
	    return -1;
	}

	r = fs_create(dir, basenm, &ino, 0);
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
    memset(&fd->fd_file.readdir_pos, 0, sizeof(fd->fd_file.readdir_pos));

    if ((flags & O_ACCMODE) != O_RDONLY) {
	struct fs_object_meta m;
	if (sys_obj_get_meta(ino.obj, &m) >= 0) {
	    m.mtime_nsec = jos_time_nsec();
	    sys_obj_set_meta(ino.obj, 0, &m);
	}
    }

    if ((flags & O_APPEND)) {
	uint64_t cursize;
	fs_getsize(ino, &cursize);
	fd->fd_offset = cursize;
    }

    return fd2num(fd);
}

static ssize_t
file_read(struct Fd *fd, void *buf, size_t n, off_t offset)
{
    ssize_t cr = fs_pread(fd->fd_file.ino, buf, n, offset);
    if (cr < 0) {
	cprintf("file_read: fs_pread: %s\n", e2s(cr));
	__set_errno(EIO);
	return -1;
    }

    return cr;
}

static ssize_t
file_write(struct Fd *fd, const void *buf, size_t n, off_t offset)
{
    ssize_t cr = fs_pwrite(fd->fd_file.ino, buf, n, offset);
    if (cr < 0) {
	cprintf("file_write: %s\n", e2s(cr));
	__set_errno(EIO);
	return -1;
    }

    if ((fd->fd_omode & O_SYNC)) {
	int r = sys_segment_sync(fd->fd_file.ino.obj, offset, n,
				 sys_pstate_timestamp());
	if (r < 0)
	    cprintf("file_write: sync: %s\n", e2s(r));
    }

    return cr;
}

static int
file_close(struct Fd *fd)
{
    return 0;
}

static int
file_stat(struct Fd *fd, struct stat64 *buf)
{
    return jos_stat(fd->fd_file.ino, buf);    
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
	cc += ROUNDUP(reclen, 8);	/* 8-byte alignment */

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
    return 1;
}

static int
file_trunc(struct Fd *fd, off_t pos)
{
    int r = fs_resize(fd->fd_file.ino, pos);
    if (r < 0) {
	cprintf("file_trunc: %s\n", e2s(r));
	__set_errno(EPERM);
	return -1;
    }

    if ((fd->fd_omode & O_SYNC)) {
	r = sys_segment_sync(fd->fd_file.ino.obj, 0, UINT64(~0),
			     sys_pstate_timestamp());
	if (r < 0)
	    cprintf("file_trunc: sync: %s\n", e2s(r));
    }

    return 0;
}

static int
file_sync(struct Fd *fd)
{
    if ((fd->fd_omode & O_SYNC))
	return 0;

    int r = sys_segment_sync(fd->fd_file.ino.obj, 0,
			     UINT64(~0), sys_pstate_timestamp());
    if (r < 0) {
	cprintf("file_sync: %s\n", e2s(r));
	__set_errno(EINVAL);
	return -1;
    }

    return 0;
}

static int
file_ioctl(struct Fd *fd, uint64_t req, va_list ap)
{
    if (req == TCGETS) {
	__set_errno(ENOTTY);
	return -1;
    } else if (req == TCSETS || req == TCSETSW || req == TCSETSF) {
	__set_errno(ENOTTY);
	return -1;
    } 
    __set_errno(EINVAL);
    return -1;
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
    .dev_trunc = file_trunc,
    .dev_sync = file_sync,
    .dev_ioctl = file_ioctl,
};

struct Dev devsymlink = {
    .dev_id = 'l',
    .dev_name = "symlink",
    /* No actual ops -- just a placeholder */
};

weak_alias(__libc_open, open);
weak_alias(__libc_open, open64);

