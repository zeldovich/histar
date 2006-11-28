#include <inc/lib.h>
#include <inc/fd.h>
#include <inc/ssld.h>
#include <inc/netd.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int
ssl_accept(int s)
{
    struct cobj_ref ssld_gate = ssld_get_gate();

    struct Fd *fd;
    int r = fd_alloc(&fd, "ssl fd");
    if (r < 0)
	return r;

    struct Fd *t = 0;
    fd_lookup(s, &t, 0, 0);
    int lwip_sock = t->fd_sock.s;

    struct ssld_op_args a;
    a.op_type = ssld_op_accept;
    a.accept.s = lwip_sock;
    a.accept.netd_gate = netd_get_gate();
    
    ssld_call(ssld_gate, &a);

    fd->fd_dev_id = devssl.dev_id;
    fd->fd_omode = O_RDWR;
    fd->fd_ssl.lwip_sock = lwip_sock;
    fd->fd_ssl.netd_sock = s;
    fd->fd_ssl.ssld_gate = ssld_gate;
   
    return fd2num(fd);
}

static ssize_t
ssl_send(struct Fd *fd, const void *buf, size_t count, int flags)
{
    if (count > ssld_buf_size)
	count = ssld_buf_size;

    struct ssld_op_args a;
    a.op_type = ssld_op_send;
    a.send.s = fd->fd_ssl.lwip_sock;
    a.send.count = count;
    a.send.flags = flags;
    memcpy(&a.send.buf[0], buf, count);
    return ssld_call(fd->fd_ssl.ssld_gate, &a);
}

static ssize_t
ssl_recv(struct Fd *fd, void *buf, size_t count, int flags)
{
    if (count > ssld_buf_size)
	count = ssld_buf_size;

    struct ssld_op_args a;
    
    a.op_type = ssld_op_recv;
    a.recv.s = fd->fd_ssl.lwip_sock;
    a.recv.count = count;
    a.recv.flags = flags;
    int r = ssld_call(fd->fd_ssl.ssld_gate, &a);
    if (r > 0)
	memcpy(buf, &a.recv.buf[0], r);
    return r;
}

static ssize_t
ssl_write(struct Fd *fd, const void *buf, size_t count, off_t offset)
{
    return ssl_send(fd, buf, count, 0);
}

static ssize_t
ssl_read(struct Fd *fd, void *buf, size_t count, off_t offset)
{
    return ssl_recv(fd, buf, count, 0);
}

static int
ssl_close(struct Fd *fd)
{
    struct ssld_op_args a;
    
    a.op_type = ssld_op_close;
    a.close.s = fd->fd_ssl.lwip_sock;
    int r = ssld_call(fd->fd_ssl.ssld_gate, &a);
    close(fd->fd_ssl.netd_sock);
    return r;
}

struct Dev devssl = 
{
    .dev_id = 'l',
    .dev_name = "ssl",
    .dev_send = ssl_send,
    .dev_recv = ssl_recv,
    .dev_write = ssl_write,
    .dev_read = ssl_read,
    .dev_close = ssl_close,
};
