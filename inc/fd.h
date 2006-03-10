#ifndef JOS_INC_FD_H
#define JOS_INC_FD_H

#include <machine/atomic.h>
#include <inc/container.h>
#include <inc/types.h>
#include <inc/fs.h>
#include <inc/pthread.h>

#include <dirent.h>
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

    int (*dev_getdents)(struct Fd *fd, struct dirent *dirbuf, int count);

    int (*dev_bind)(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
    int (*dev_listen)(struct Fd *fd, int backlog);
    int (*dev_accept)(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen);
    int (*dev_connect)(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
    int (*dev_getsockname)(struct Fd *fd, struct sockaddr *name, socklen_t *namelen);
    int (*dev_getpeername)(struct Fd *fd, struct sockaddr *name, socklen_t *namelen);
    int (*dev_setsockopt)(struct Fd *fd, int level, int optname,
			  const void *optval, socklen_t optlen);
    int (*dev_getsockopt)(struct Fd *fd, int level, int optname,
			  void *optval, socklen_t *optlen);
};

struct Fd
{
    int fd_dev_id;
    off_t fd_offset;
    int fd_omode;
    int fd_immutable;
    int fd_isatty;

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
	    struct fs_readdir_pos readdir_pos;
	} fd_file;

	struct {
	    char buf[512];
	    uint32_t read_ptr;	// read at this offset
	    uint64_t bytes;	// # bytes in circular buffer
	    pthread_mutex_t mu;
	} fd_pipe;
    };
};

char*	fd2data(struct Fd *fd);
int	fd2num(struct Fd *fd);
int	fd_alloc(uint64_t container, struct Fd **fd_store, const char *name);
int	fd_close(struct Fd *fd);
int	fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp);
int	fd_move(int fdnum, uint64_t container);
void	fd_give_up_privilege(int fdnum);
int	fd_set_isatty(int fdnum, int isit);
int	dev_lookup(int devid, struct Dev **dev_store);

extern struct Dev devcons;
extern struct Dev devsock;
extern struct Dev devfile;
extern struct Dev devpipe;

int	dup2_as(int oldfd, int newfd, struct cobj_ref target_as);
void	close_all(void);
ssize_t	readn(int fd, void *buf, size_t nbytes);

#endif
