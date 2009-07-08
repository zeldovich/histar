#ifndef JOS_INC_UDSIMPL_H
#define JOS_INC_UDSIMPL_H

int uds_socket(int domain, int type, int protocol);
int uds_close(struct Fd *fd);
int uds_bind(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
int uds_listen(struct Fd *fd, int backlog);
int uds_connect(struct Fd *fd, const struct sockaddr *addr, socklen_t addrlen);
int uds_accept(struct Fd *fd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t uds_write(struct Fd *fd, const void *buf, size_t count, off_t offset);
ssize_t uds_read(struct Fd *fd, void *buf, size_t count, off_t offset);
int uds_setsockopt(struct Fd *fd, int level, int optname, const void *optval, socklen_t optlen);

int uds_addref(struct Fd *fd, uint64_t ct);
int uds_unref(struct Fd *fd);

int uds_probe(struct Fd *fd, dev_probe_t probe);
int uds_statsync(struct Fd *fd, dev_probe_t probe, struct wait_stat *wstat, int wslot_avail);
int uds_shutdown(struct Fd *fd, int how);
int uds_ioctl(struct Fd *fd, uint64_t req, va_list ap);

ssize_t uds_recvfrom(struct Fd *fd, void *buf, size_t len, int flags,
    struct sockaddr *from, socklen_t *fromlen);
ssize_t uds_sendto(struct Fd *fd, const void *buf, size_t len, int flags,
    const struct sockaddr *to, socklen_t tolen);

#endif
