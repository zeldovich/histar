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

int uds_onfork(struct Fd *fd, uint64_t ct);

#endif
