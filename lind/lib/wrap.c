#include <linux/syscalls.h>
#include <linuxsyscall.h>

/* XXX
 * - use kernel versions of system calls (ie kernel_setsockopt)?
 */

long 
linux_socket(int domain, int type, int protocol)
{
    return sys_socket(domain, type, protocol);
}

long
linux_connect(int sockfd, struct sockaddr *serv_addr, int addrlen)
{
    return sys_connect(sockfd, serv_addr, addrlen);
}

long 
linux_bind(int sockfd, struct sockaddr *my_addr, int addrlen)
{
    return sys_bind(sockfd, my_addr, addrlen);
}

long 
linux_listen(int sockfd, int backlog)
{
    return sys_listen(sockfd, backlog);
}

long
linux_accept(int sockfd, struct sockaddr *addr, int *addrlen)
{
    return sys_accept(sockfd, addr, addrlen);
}

long 
linux_ioctl(unsigned int fd, unsigned int cmd, void *arg)
{
    return sys_ioctl(fd, cmd, (unsigned long)arg);
}

long
linux_close(unsigned int fd)
{
    return sys_close(fd);
}

long 
linux_socketpair(int domain, int type, int protocol, int sv[2])
{
    return sys_socketpair(domain, type, protocol, sv);
}

long
linux_setsockopt(int fd, int level, int optname,
		 char *optval, int optlen)
{
    return sys_setsockopt(fd, level, optname, optval, optlen);
}

long
linux_getsockopt(int fd, int level, int optname,
		 char *optval, int *optlen)
{
    return sys_getsockopt(fd, level, optname, optval, optlen);
}

long
linux_getsockname(int fd, struct sockaddr *sa, int *len)
{
    return sys_getsockname(fd, sa, len);
}

long
linux_getpeername(int fd, struct sockaddr *sa, int *len)
{
    return sys_getpeername(fd, sa, len);
}

ssize_t 
linux_read(unsigned int fd, char *buf, size_t count)
{
    return sys_read(fd, buf, count);
}

ssize_t 
linux_write(unsigned int fd, char *buf, size_t count)
{
    return sys_write(fd, buf, count);
}

long
linux_getpid(void)
{
    return sys_getpid();

}

long 
linux_kill(int pid, int sig)
{
    return sys_kill(pid, sig);
}

unsigned long 
linux_signal(int sig, void *handler)
{
    return sys_signal(sig, handler);
}

long
linux_restart_syscall(void)
{
    return sys_restart_syscall();
}

long 
linux_nanosleep(struct timespec *rqtp, struct timespec *rmtp)
{
    return sys_nanosleep(rqtp, rmtp);
}

long
linux_pause(void)
{
    return sys_pause();
}

long 
linux_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    return sys_fcntl(fd, cmd, arg);
}

long 
linux_sigprocmask(int how, void *set, void *oset)
{
    return sys_sigprocmask(how, set, oset);
}

long 
linux_select(int n, fd_set *inp, fd_set *outp,
	     fd_set *exp, struct timeval *tvp)
{
    return sys_select(n, inp, outp, exp, tvp);
}

long 
linux_sendto(int fd, void *buf, size_t len, unsigned flags,
	     struct sockaddr *to, int tolen)
{
    return sys_sendto(fd, buf, len, flags, to, tolen);
}

long 
linux_send(int fd, void *buf, size_t len, unsigned flags)
{
    return sys_send(fd, buf, len, flags);
}

long 
linux_open(const char *filename, int flags, int mode)
{
    return sys_open(filename, flags, mode);
}

long 
linux_mount(char *dev_name, char *dir_name,
	    char *type, unsigned long flags,
	    void *data)
{
    return sys_mount(dev_name, dir_name, type, flags, data);
}

long 
linux_mkdir(const char *pathname, int mode)
{
    return sys_mkdir(pathname, mode);
}

off_t 
linux_lseek(unsigned int fd, off_t offset, unsigned int origin)
{
    return sys_lseek(fd, offset, origin);
}
