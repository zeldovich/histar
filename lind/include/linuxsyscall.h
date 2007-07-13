#ifndef LINUX_ARCH_INCLUDE_LINUXSYSCALL_H
#define LINUX_ARCH_INCLUDE_LINUXSYSCALL_H

struct timespec;
struct timeval;
struct sockaddr;

long linux_socket(int domain, int type, int protocol);
long linux_connect(int sockfd, struct sockaddr *serv_addr, int addrlen);
long linux_bind(int sockfd, struct sockaddr *my_addr, int addrlen);
long linux_listen(int sockfd, int backlog);
long linux_accept(int sockfd, struct sockaddr *addr, int *addrlen);
long linux_close(unsigned int fd);
long linux_socketpair(int domain, int type, int protocol, int sv[2]);
long linux_setsockopt(int fd, int level, int optname,
		      char *optval, int optlen);
long linux_getsockopt(int fd, int level, int optname,
		      char *optval, int *optlen);
long linux_getsockname(int fd, struct sockaddr *sa, int *len);
long linux_getpeername(int fd, struct sockaddr *sa, int *len);
long linux_shutdown(int fd, int how);

ssize_t linux_read(unsigned int fd, char *buf, size_t count);
ssize_t linux_write(unsigned int fd, char *buf, size_t count);
long linux_open(const char *filename, int flags, int mode);

long linux_send(int fd, void *buf, size_t len, unsigned flags);
long linux_sendto(int fd, void *buf, size_t len, unsigned flags,
		  struct sockaddr *to, int tolen);

long linux_getpid(void);
long linux_kill(int pid, int sig);
long linux_pause(void);
long linux_restart_syscall(void);
long linux_nanosleep(struct timespec *rqtp, struct timespec *rmtp);

long linux_ioctl(unsigned int fd, unsigned int cmd, void *arg);
long linux_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);

long linux_sigprocmask(int how, void *set, void *oset);
unsigned long linux_signal(int sig, void *handler);

long linux_select(int n, fd_set *inp, fd_set *outp,
		  fd_set *exp, struct timeval *tvp);

long linux_mount(char *dev_name, char *dir_name,
		 char *type, unsigned long flags,
		 void *data);

long linux_mkdir(const char *pathname, int mode);

off_t linux_lseek(unsigned int fd, off_t offset, unsigned int origin);


#endif
