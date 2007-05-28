#include <inc/fd.h>
#include <inc/fs.h>
#include <inc/lib.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/syscall.h>
#include <inc/stat.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>

int
jos_stat(struct fs_inode ino, struct stat64 *buf)
{
    int type = sys_obj_get_type(ino.obj);
    if (type < 0) {
	cprintf("file_stat: get_type: %s\n", e2s(type));
	__set_errno(EIO);
	return -1;
    }

    uint32_t ftype = 0;

    struct fs_object_meta meta;
    int r = sys_obj_get_meta(ino.obj, &meta);
    if (r >= 0) {
	buf->st_mtime = meta.mtime_nsec / NSEC_PER_SECOND;
	buf->st_mtimensec = meta.mtime_nsec % NSEC_PER_SECOND;
	buf->st_ctime = meta.ctime_nsec / NSEC_PER_SECOND;
	buf->st_ctimensec = meta.ctime_nsec % NSEC_PER_SECOND;

	if (meta.dev_id) {
	    if (meta.dev_id == devsymlink.dev_id)
		ftype = __S_IFLNK;
	    else if (meta.dev_id != devfile.dev_id)
		ftype = __S_IFCHR;
	}
    }

    buf->st_mode = S_IRWXU;
    buf->st_ino = ino.obj.object;
    if (type == kobj_container) {
	if (!ftype)
	    ftype = __S_IFDIR;
    } else {
	if (!ftype)
	    ftype = __S_IFREG;

	uint64_t len;
	r = fs_getsize(ino, &len);
	if (r < 0) {
	    cprintf("file_stat: getsize: %s\n", e2s(r));
	    __set_errno(EIO);
	    return -1;
	}
	buf->st_size = len;
    }

    buf->st_mode |= ftype;
    return 0;
}
