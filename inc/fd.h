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

/* Maximum number of file descriptors a program may hold open concurrently */
#define MAXFD		32

/* pre-declare for forward references */
struct Fd;
struct Dev;

typedef enum
{
    dev_probe_read,
    dev_probe_write,    
} dev_probe_t ;

struct Dev
{
    int dev_id;
    const char *dev_name;

    ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len, off_t offset);
    ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len, off_t offset);
    ssize_t (*dev_recv)(struct Fd *fd, void *buf, size_t len, int flags);
    ssize_t (*dev_send)(struct Fd *fd, const void *buf, size_t len, int flags);
    int (*dev_close)(struct Fd *fd);
    int (*dev_seek)(struct Fd *fd, off_t pos);
    int (*dev_trunc)(struct Fd *fd, off_t length);
    int (*dev_stat)(struct Fd *fd, struct stat *buf);
    int (*dev_probe)(struct Fd *fd, dev_probe_t probe);
    int (*dev_sync)(struct Fd *fd);

    ssize_t (*dev_getdents)(struct Fd *fd, struct dirent *dirbuf, size_t nbytes);

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
    int (*dev_shutdown)(struct Fd *fd, int how);
};

struct Fd
{
    int fd_dev_id;
    off_t fd_offset;
    int fd_omode;
    int fd_immutable;
    int fd_isatty;
    int fd_private;

    /* handles for this fd */
    uint64_t fd_grant;
    uint64_t fd_taint;

    atomic_t fd_ref;

    union {
	struct {
	} fd_dev_state;

	struct {
	    struct cobj_ref netd_gate;
	    int s;
	} fd_sock;

	struct {
	    struct fs_inode ino;
	    struct fs_readdir_pos readdir_pos;
	} fd_file;

	struct {
	    uint64_t bytes;	/* # bytes in circular buffer */
	    uint32_t read_ptr;	/* read at this offset */
	    char reader_waiting;
	    char writer_waiting;
	    pthread_mutex_t mu;
	    char buf[4000];
	} fd_pipe;

	struct {
        struct cobj_ref bipipe_seg;
        int bipipe_a;
	} fd_bipipe;

	struct {
	    struct cobj_ref tun_seg;
	    int tun_a;
	} fd_tun;
    };
};

char*	fd2data(struct Fd *fd);
int	fd2num(struct Fd *fd);
int	fd_alloc(struct Fd **fd_store, const char *name);
int	fd_close(struct Fd *fd);
int	fd_lookup(int fdnum, struct Fd **fd_store, struct cobj_ref *objp, uint64_t *flagsp);
int	fd_setflags(struct Fd *fd, struct cobj_ref obj, uint64_t newflags);
void	fd_give_up_privilege(int fdnum);
int	fd_set_isatty(int fdnum, int isit);
int	dev_lookup(int devid, struct Dev **dev_store);

/* Allocates individual handles for this FD */
int	fd_make_public(int fdnum);

extern struct Dev devcons;	/* type 'c' */
extern struct Dev devsock;	/* type 's' */
extern struct Dev devfile;	/* type 'f' */
extern struct Dev devpipe;	/* type 'p' */
extern struct Dev devtun;	/* type 't' */
extern struct Dev devbipipe;	/* type 'b' */
extern struct Dev devrand;  	/* type 'r' */
extern struct Dev devzero;	/* type 'z' */
extern struct Dev devnull;	/* type 'n' */

int	dup2_as(int oldfd, int newfd,
		struct cobj_ref target_as, uint64_t target_ct);
void	close_all(void);
ssize_t	readn(int fd, void *buf, size_t nbytes);

#endif
