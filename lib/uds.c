#include <inc/syscall.h>
#include <inc/lib.h>
#include <inc/uds.h>
#include <inc/bipipe.h>
#include <inc/error.h>
#include <bits/unimpl.h>
#include <bits/udsgate.h>

#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int
uds_socket(int domain, int type, int protocol)
{
    struct Fd *fd;
    int r = fd_alloc(&fd, "unix-domain");
    if (r < 0) {
	errno = ENOMEM;
	return -1;
    }
    memset(&fd->fd_uds, 0, sizeof(fd->fd_uds));
    
    fd->fd_dev_id = devuds.dev_id;
    fd->fd_omode = O_RDWR;

    fd->fd_uds.uds_backlog = 16;
    return fd2num(fd);
}

static int
uds_close(struct Fd *fd)
{
    int r;

    if (fd->fd_uds.uds_gate.object) {
	r = sys_obj_unref(fd->fd_uds.uds_gate);
	if (r < 0)
	    cprintf("uds_close: unable to unref gate: %s\n", e2s(r));
    }

    return (*devbipipe.dev_close)(fd);
}

static int
uds_bind(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct fs_inode ino;

    if (fd->fd_uds.uds_file.obj.object != 0) {
	errno = EINVAL;
	return -1;
    }

    char *pn = (char *)addr->sa_data;
    r = fs_namei(pn, &ino);
    if (r == -E_NOT_FOUND) {
	char *pn2;
	const char *dn, *fn;
	struct fs_inode dir_ino;
	pn2 = strdup(pn);
	fs_dirbase(pn2, &dn, &fn);
	r = fs_namei(dn, &dir_ino);
	if (r < 0) {
	    free(pn2);
	    errno = ENOTDIR;
	    return -1;
	}

	uint64_t label_ent[4];
	struct ulabel label = { .ul_size = 4, .ul_ent = &label_ent[0] };
	label.ul_default = 1;
	label.ul_nent = 0;
	if (start_env->user_grant)
	    label_set_level(&label, start_env->user_grant, 0, 1);
	
	r = fs_mknod(dir_ino, fn, devuds.dev_id, 0, &ino, &label);
	if (r < 0) {
	    free(pn2);
	    errno = EACCES;
	    return -1;
	}

	free(pn2);
    } else if (r < 0) {
	errno = EINVAL;
	return -1;
    }
    
    fd->fd_uds.uds_file = ino;
    return 0;
}

static int
uds_listen(struct Fd *fd, int backlog)
{
    int r;

    if (backlog > 16) {
	errno = EINVAL;
	return -1;
    }
    fd->fd_uds.uds_backlog = backlog;
    fd->fd_uds.uds_listen = 1;
    
    if (!fd->fd_uds.uds_gate.object) {
	r = uds_gate_new(fd, start_env->shared_container, &fd->fd_uds.uds_gate);
	if (r < 0) {
	    fd->fd_uds.uds_listen = 0;
	    errno = EACCES;
	    return -1;
	}
    }
    
    r = fs_pwrite(fd->fd_uds.uds_file, &fd->fd_uds.uds_gate, 
		  sizeof(fd->fd_uds.uds_gate), 0);
    if (r < 0) {
	fd->fd_uds.uds_listen = 0;
	errno = EACCES;
	return -1;
    }

    return 0;
}

static int
uds_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen)
{
    int r;
    
    r = uds_gate_accept(fd);
    if (r < 0) {
	errno = EINVAL;
	return -1;
    }
        
    return r;
}

static int
uds_connect(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen)
{
    int r;
    struct cobj_ref gate;
    struct fs_inode ino;
    char *pn = (char *)addr->sa_data;
    
    r = fs_namei(pn, &ino);
    if (r < 0) {
	errno = ENOENT;
	return -1;
    }
    
    struct fs_object_meta m;
    r = sys_obj_get_meta(ino.obj, &m);
    if (r < 0) {
	errno = EACCES;
	return -1;
    }

    if (m.dev_id != devuds.dev_id) {
	errno = ECONNREFUSED;
	return -1;
    }

    r = fs_pread(ino, &gate, sizeof(gate), 0);
    if (r < 0) {
	errno = EACCES;
	return -1;
    } else if (r != sizeof(gate)) {
	/* a uds dev was just created, and nobody is listening yet */
	errno = ECONNREFUSED;
	return -1;
    }

    r = uds_gate_connect(fd, gate);
    if (r < 0) {
	errno = EACCES;
	return -1;
    }
        
    return 0;
}

static ssize_t
uds_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return (*devbipipe.dev_write)(fd, buf, count, 0); 
}

static ssize_t
uds_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return (*devbipipe.dev_read)(fd, buf, count, 0);
}

struct Dev devuds = {
    .dev_id = 'u',
    .dev_name = "unix-domain",
    .dev_read = &uds_read,
    .dev_write = &uds_write,
    .dev_close = &uds_close,
    .dev_connect = &uds_connect,
    .dev_bind = &uds_bind,
    .dev_listen = &uds_listen,
    .dev_accept = &uds_accept,
};
