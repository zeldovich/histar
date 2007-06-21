#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <inttypes.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <bits/unimpl.h>

static int
err_jos2libc(int64_t r)
{
    if (r >= 0)
	return 0;

    if (r == -E_LABEL)
	__set_errno(EACCES);
    else if (r == -E_NOT_FOUND)
	__set_errno(ENOENT);
    else if (r == -E_NO_MEM)
	__set_errno(ENOMEM);
    
    return -1;
}

int
mkdir(const char *pn, mode_t mode)
{
    struct fs_inode dir;
    int r = fs_namei(pn, &dir);
    if (r == 0) {
	__set_errno(EEXIST);
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

    r = fs_namei(dirname, &dir);
    if (r < 0)
        goto done;

    struct fs_inode ndir;
    r = fs_mkdir(dir, basenm, &ndir, 0);

done:
    free(pn2);
    switch (r) {
        case 0:
            return 0;
        case -E_INVAL:
        case -E_LABEL:
            __set_errno(EACCES);
            return -1;
        case -E_NOT_FOUND:
            __set_errno(ENOTDIR);
            return -1;
        default:
            return -1;
    }
}

int 
link(const char *oldpath, const char *newpath)
{
    char *pn = strdup(newpath);
    const char *dirname, *basenm;
    fs_dirbase(pn, &dirname, &basenm);

    struct fs_inode dir_ino;
    int r = fs_namei(dirname, &dir_ino);
    if (r < 0)
	goto err;

    struct fs_inode ino;
    r = fs_namei(oldpath, &ino);
    if (r < 0)
	goto err;

    r = fs_link(dir_ino, basenm, ino);
    if (r < 0 && r != -E_LABEL) {
	free(pn);
	__set_errno(EXDEV);
	return -1;
    }

 err:
    free(pn);
    return err_jos2libc(r);
}

int 
symlink(const char *oldpath, const char *newpath)
{
    char *pn = strdup(newpath);
    const char *dirname, *basenm;
    fs_dirbase(pn, &dirname, &basenm);

    struct fs_inode dir_ino;
    int r = fs_namei(dirname, &dir_ino);
    if (r < 0)
	goto err;

    struct fs_inode ino;
    r = fs_mknod(dir_ino, basenm, devsymlink.dev_id, 0, &ino, 0);
    if (r < 0)
	goto err;

    ssize_t cc = fs_pwrite(ino, oldpath, strlen(oldpath), 0);
    if (cc < 0) {
	fs_remove(dir_ino, basenm, ino);
	r = cc;
	goto err;
    }

 err:
    free(pn);
    return err_jos2libc(r);
}

int 
mknod(const char *pathname, mode_t mode, dev_t dev)
{
    int r;
    
    char *pn = strdup(pathname);
    const char *dirname;
    const char *basenm;    
    fs_dirbase(pn, &dirname, &basenm);

    struct fs_inode dir_ino;
    r = fs_namei(dirname, &dir_ino);
    if (r < 0) {
	free(pn);
	return err_jos2libc(r);
    }

    // approximate the mode 
    uint64_t ent[8];
    struct ulabel ul =
	{ .ul_size = sizeof(ent) / sizeof(uint64_t), 
	  .ul_ent = ent,
	  .ul_default = 1 };

    if (!(mode & S_IROTH))
	label_set_level(&ul, start_env->user_taint, 3, 0);
    if (!(mode & S_IWOTH))
	label_set_level(&ul, start_env->user_grant, 0, 0);

    uint32_t dev_id;

    if (mode | S_IFREG)
	dev_id = devfile.dev_id;
    else if ((mode | S_IFCHR))
	dev_id = dev;
    else {
	free(pn);
	set_enosys();
	return -1;
    }

    struct fs_inode ino;
    r = fs_mknod(dir_ino, basenm, dev_id, 0, &ino, &ul);
    free(pn);
    return err_jos2libc(r);
}

int
unlink(const char *pn)
{
    char *pn2 = malloc(strlen(pn) + 1);
    if (pn2 == 0)
	return -E_NO_MEM;

    strcpy(pn2, pn);
    const char *dirname, *basenm;
    fs_dirbase(pn2, &dirname, &basenm);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0)
	goto err;

    struct fs_inode f;
    r = fs_namei_flags(pn, &f, NAMEI_LEAF_NOEVAL);
    if (r < 0)
	goto err;

    r = fs_remove(dir, basenm, f);

 err:
    free(pn2);
    return err_jos2libc(r);
}

int
rmdir(const char *pn)
{
    return unlink(pn);
}

int
rename(const char *src, const char *dst)
{
    char *src2 = strdup(src);
    const char *src_dir, *src_base;
    fs_dirbase(src2, &src_dir, &src_base);

    char *dst2 = strdup(dst);
    const char *dst_dir, *dst_base;
    fs_dirbase(dst2, &dst_dir, &dst_base);

    struct fs_inode src_dir_ino;
    int r = fs_namei(src_dir, &src_dir_ino);
    if (r < 0)
	goto err;

    struct fs_inode dst_dir_ino;
    r = fs_namei(dst_dir, &dst_dir_ino);
    if (r < 0)
	goto err;

    if (src_dir_ino.obj.object != dst_dir_ino.obj.object) {
	free(src2);
	free(dst2);
	__set_errno(EXDEV);
	return -1;
    }

    struct fs_inode f;
    r = fs_namei_flags(src, &f, NAMEI_LEAF_NOEVAL);
    if (r < 0)
	goto err;

    r = fs_rename(src_dir_ino, src_base, dst_base, f);

 err:
    free(src2);
    free(dst2);
    return err_jos2libc(r);
}

int
chdir(const char *pn)
{
    struct fs_inode dir;
    int r = fs_namei(pn, &dir);
    if (r < 0)
	return err_jos2libc(r);

    start_env->fs_cwd = dir;
    return 0;
}

mode_t
umask(mode_t mask)
{
    return 0;
}

int
utime (const char *file, const struct utimbuf *file_times)
{
    return 0;
}

int
jos_stat64_to_stat(struct stat64 *src, struct stat *dst)
{
#define F(n)				\
    do {				\
	dst->n = src->n;		\
	if (dst->n != src->n) {		\
	    __set_errno(ERANGE);	\
	    return -1;			\
	}				\
    } while (0)

    F(st_dev);	    F(st_ino);	    F(st_nlink);    F(st_mode);
    F(st_uid);	    F(st_gid);	    F(st_rdev);	    F(st_size);
    F(st_blksize);  F(st_blocks);   F(st_atime);    F(st_mtime);
    F(st_ctime);    F(st_atimensec);F(st_mtimensec);F(st_ctimensec);
#undef F

    return 0;
}

static int
stat_common(const char *file_name, struct stat64 *buf, uint32_t namei_flags)
{
    struct fs_inode ino;
    int r = fs_namei_flags(file_name, &ino, namei_flags);
    if (r < 0)
	return err_jos2libc(r);

    int type = sys_obj_get_type(ino.obj);
    if (type == kobj_container || type == kobj_segment) {
	struct fs_object_meta m;
	struct Dev *dev;
	r = sys_obj_get_meta(ino.obj, &m);
	if (m.dev_id != devfile.dev_id && m.dev_id != devsymlink.dev_id &&
	    (dev_lookup(m.dev_id, &dev) >= 0) && dev->dev_stat)
	{
	    // call dev_stat via fstat below if have a custom stat
	    ;
	} else {
	    memset(buf, 0, sizeof(*buf));
	    buf->st_dev = 1;	// some apps want a non-zero value
	    buf->st_nlink = 1;
	    return jos_stat(ino, buf);
	}
    }

    int fd = open(file_name, O_RDONLY);
    if (fd < 0)
	return fd;

    r = fstat64(fd, buf);

    int esave = errno;
    close(fd);
    __set_errno(esave);

    return r;
}

int
stat64(const char *file_name, struct stat64 *buf)
{
    return stat_common(file_name, buf, 0);
}

int
stat(const char *file_name, struct stat *buf)
{
    struct stat64 s64;
    int r = stat64(file_name, &s64);
    if (r < 0)
	return r;

    return jos_stat64_to_stat(&s64, buf);
}

int
lstat64(const char *file_name, struct stat64 *buf)
{
    return stat_common(file_name, buf, NAMEI_LEAF_NOEVAL);
}

int 
lstat(const char *file_name, struct stat *buf)
{
    struct stat64 s64;
    int r = lstat64(file_name, &s64);
    if (r < 0)
	return r;

    return jos_stat64_to_stat(&s64, buf);
}

int
access(const char *pn, int mode)
{
    int fd = open(pn, O_RDONLY);
    if (fd < 0)
	return -1;

    // XXX lie about it, for now..
    close(fd);
    return 0;
}

ssize_t
readlink(const char *pn, char *buf, size_t bufsize)
{
    struct fs_inode ino;
    int r = fs_namei_flags(pn, &ino, NAMEI_LEAF_NOEVAL);
    if (r < 0)
	return err_jos2libc(r);

    struct fs_object_meta m;
    r = sys_obj_get_meta(ino.obj, &m);
    if (r < 0)
	return err_jos2libc(r);

    if (m.dev_id != devsymlink.dev_id) {
	__set_errno(EINVAL);
	return -1;
    }

    ssize_t cc = fs_pread(ino, buf, bufsize, 0);
    if (cc < 0)
	return err_jos2libc(cc);
    return cc;
}

char *
getcwd(char *buf, size_t size)
{
    int alloc = 0;

    if (buf == 0) {
	if (size == 0)
	    size = 256;

	alloc = 1;
	buf = malloc(size);
	if (buf == 0)
	    return 0;
    }

    char tmpbuf[256];
    tmpbuf[0] = '\0';

    struct fs_inode ino = start_env->fs_cwd;
    while (ino.obj.object != start_env->fs_root.obj.object) {
	int64_t parent_ct = sys_container_get_parent(ino.obj.object);
	if (parent_ct < 0) {
	    cprintf("getcwd: cannot get parent: %s\n", e2s(parent_ct));
	    goto err;
	}

	struct fs_inode parent_ino;
	fs_get_root(parent_ct, &parent_ino);

	struct fs_readdir_state s;
	int r = fs_readdir_init(&s, parent_ino);
	if (r < 0) {
	    cprintf("getcwd: fs_readdir_init: %s\n", e2s(r));
	    goto err;
	}

	for (;;) {
	    struct fs_dent de;
	    r = fs_readdir_dent(&s, &de, 0);
	    if (r < 0) {
		cprintf("getcwd: fs_readdir_dent: %s\n", e2s(r));
		fs_readdir_close(&s);
		goto err;
	    }

	    if (r == 0) {
		cprintf("getcwd: fs_readdir_dent: cannot find %"PRIu64" in parent %"PRIu64"\n",
			ino.obj.object, parent_ct);
		fs_readdir_close(&s);
		goto err;
	    }

	    if (de.de_inode.obj.object == ino.obj.object) {
		char tmp2[sizeof(tmpbuf)];

		if (tmpbuf[0] == '\0') {
		    snprintf(&tmp2[0], sizeof(tmp2), "%s", &de.de_name[0]);
		} else {
		    snprintf(&tmp2[0], sizeof(tmp2), "%s/%s", &de.de_name[0], &tmpbuf[0]);
		}

		memcpy(&tmpbuf[0], &tmp2[0], sizeof(tmpbuf));
		break;
	    }
	}

	fs_readdir_close(&s);
	ino = parent_ino;
    }

    snprintf(buf, size, "/%s", &tmpbuf[0]);
    return buf;

err:
    if (alloc)
	free(buf);
    return 0;
}

int
chown(const char *path, uid_t owner, gid_t group)
{
    return 0;
}

int
chmod(const char *path, mode_t mode)
{
    return 0;
}

int 
creat(const char *pathname, mode_t mode)
{
    return open(pathname, O_CREAT|O_WRONLY|O_TRUNC, mode);
}

int
chroot(const char *pathname)
{
    struct fs_inode dir;
    int r = fs_namei(pathname, &dir);
    if (r < 0) {
	__set_errno(ENOENT);
	return -1;
    }

    start_env->fs_root = dir;
    return 0;
}

int 
mkfifo (const char *path, mode_t mode)
{
    set_enosys();
    return -1;
}
