#ifndef JOS_INC_FD_H
#define JOS_INC_FD_H

#include <machine/atomic.h>
#include <inc/container.h>
#include <inc/types.h>
#include <inc/fs.h>

#include <arpa/inet.h>

struct stat;

// pre-declare for forward references
struct Fd;
struct Dev;

struct Dev
{
	int dev_id;
	const char *dev_name;

	ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len, off_t offset);
	ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len, off_t offset);
	int (*dev_close)(struct Fd *fd);
	int (*dev_seek)(struct Fd *fd, off_t pos);
	int (*dev_trunc)(struct Fd *fd, off_t length);
	int (*dev_stat)(struct Fd *fd, struct stat *buf);

	int (*dev_bind)(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
	int (*dev_listen)(struct Fd *fd, int backlog);
	int (*dev_accept)(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen);
	int (*dev_connect)(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
};

struct Fd
{
	int fd_dev_id;
	off_t fd_offset;
	int fd_omode;
	int fd_immutable;

	// handles for this fd
	uint64_t fd_grant;
	uint64_t fd_taint;

	atomic_t fd_ref;

	union {
		struct {
			int s;
		} fd_sock;

		struct {
			struct fs_inode ino;
		} fd_file;
	};
};

char*	fd2data(struct Fd *fd);
int	fd2num(struct Fd *fd);
int	fd_alloc(uint64_t container, struct Fd **fd_store, const char *name);
int	fd_close(struct Fd *fd);
int	fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp);
int	fd_move(int fdnum, uint64_t container);
void	fd_give_up_privilege(int fdnum);
int	dev_lookup(int devid, struct Dev **dev_store);

extern struct Dev devcons;
extern struct Dev devsock;
extern struct Dev devfile;

int	dup2_as(int oldfd, int newfd, struct cobj_ref target_as);
void	close_all(void);
ssize_t	readn(int fd, void *buf, size_t nbytes);

#endif
