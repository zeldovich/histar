#ifndef JOS_INC_FD_H
#define JOS_INC_FD_H

#include <machine/atomic.h>
#include <inc/container.h>
#include <inc/types.h>

#include <lwip/inet.h>
#include <lwip/sockets.h>

// pre-declare for forward references
struct Fd;
struct Dev;

struct Dev
{
	int dev_id;
	const char *dev_name;

	int (*dev_read)(struct Fd *fd, void *buf, size_t len, off_t offset);
	int (*dev_write)(struct Fd *fd, const void *buf, size_t len, off_t offset);
	int (*dev_close)(struct Fd *fd);
	int (*dev_seek)(struct Fd *fd, off_t pos);
	int (*dev_trunc)(struct Fd *fd, off_t length);

	int (*dev_bind)(struct Fd *fd, struct sockaddr *addr, socklen_t addrlen);
	int (*dev_listen)(struct Fd *fd, int backlog);
	int (*dev_accept)(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen);
	int (*dev_connect)(struct Fd *fd, struct sockaddr *addr, socklen_t addrlen);
};

struct Fd
{
	int fd_dev_id;
	off_t fd_offset;
	int fd_omode;
	int fd_immutable;

	atomic_t fd_ref;

	union {
		struct {
			int s;
		} fd_sock;
	};
};

char*	fd2data(struct Fd *fd);
int	fd2num(struct Fd *fd);
int	fd_alloc(uint64_t container, struct Fd **fd_store, const char *name);
int	fd_close(struct Fd *fd);
int	fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp);
int	fd_map_as(struct cobj_ref as, struct cobj_ref fd_seg, int fdnum);
int	fd_unmap(struct Fd *fd);
int	dev_lookup(int devid, struct Dev **dev_store);

extern struct Dev devcons;
extern struct Dev devsock;

#define O_RDONLY	0x0000
#define O_WRONLY	0x0001
#define O_RDWR		0x0002
#define O_ACCMODE	(O_RDONLY | O_RDWR | O_WRONLY)

#endif
