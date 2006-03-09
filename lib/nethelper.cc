#include <inc/nethelper.hh>
#include <inc/error.hh>
#include <new>

extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/fs.h>
#include <inc/syscall.h>
#include <inc/fd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
}

url::url(const char *s) : host_(0), path_(0)
{
    static const char *prefix = "http://";
    if (strncmp(s, prefix, strlen(prefix)))
	throw basic_exception("bad URL type: %s", s);

    const char *h = s + strlen(prefix);
    char *slash = strchr(h, '/');
    if (slash == 0)
	throw basic_exception("poorly formatted URL: %s", s);

    size_t hostlen = slash - h;
    host_ = (char *) malloc(hostlen + 1);
    path_ = (char *) malloc(strlen(slash) + 1);

    if (!host_ || !path_)
	throw std::bad_alloc();

    strncpy(host_, h, hostlen);
    host_[hostlen] = '\0';
    strcpy(path_, slash);
}

url::~url()
{
    if (host_)
	free(host_);
    if (path_)
	free(path_);
}

tcpconn::tcpconn(const char *hostname, uint16_t port) : fd_(-1)
{
    uint32_t ip = 0;
    const char *p = hostname;

    for (int i = 0; host_table[i].alias; i++)
	if (!strcmp(p, host_table[i].alias))
	    p = host_table[i].name;

    for (int i = 0; i < 4; i++) {
	if (p == 0)
	    throw basic_exception("bad ip address: %s", hostname);

	int v = atoi(p);
	p = strchr(p, '.');
	if (p != 0)
	    p++;

	if (v < 0 || v >= 256)
	    throw basic_exception("bad ip octet: %s", hostname);

	ip = (ip << 8) | v;
    }

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(ip);

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0)
	throw error(fd_, "socket");

    int r = connect(fd_, (struct sockaddr *) &sin, sizeof(sin));
    if (r < 0)
	throw error(r, "connect");
}

tcpconn::tcpconn(int fd)
{
    fd_ = fd;
}

tcpconn::~tcpconn()
{
    if (fd_ >= 0)
	close(fd_);
}

void
tcpconn::write(const char *buf, size_t count)
{
    size_t done = 0;

    while (done < count) {
	ssize_t r = ::write(fd_, buf + done, count - done);
	if (r <= 0)
	    throw error(r, "cannot write");
	done += r;
    }
}

size_t
tcpconn::read(char *buf, size_t len)
{
    ssize_t r = ::read(fd_, buf, len);
    if (r < 0)
	throw error(r, "cannot read");

    return r;
}

lineparser::lineparser(tcpconn *tc) : tc_(tc)
{
    pos_ = 0;
    size_ = 4096;
    valid_ = 0;
    buf_ = (char *) malloc(size_);
    if (buf_ == 0)
	throw std::bad_alloc();
}

lineparser::~lineparser()
{
    if (buf_)
	free(buf_);
}

size_t
lineparser::read(char *buf, size_t len)
{
    if (valid_ == pos_)
	refill();

    size_t queued = valid_ - pos_;
    if (queued > len)
	queued = len;
    memcpy(buf, &buf_[pos_], queued);
    pos_ += queued;
    return queued;
}

const char *
lineparser::read_line()
{
retry:
    char *base = &buf_[pos_];
    char *newline = strstr(base, "\r\n");
    if (newline == 0) {
	size_t cc = refill();
	if (cc > 0)
	    goto retry;

	return 0;
    }

    *newline = '\0';
    pos_ += (newline - base) + 2;
    return base;
}

size_t
lineparser::refill()
{
    memmove(&buf_[0], &buf_[pos_], valid_ - pos_);
    valid_ -= pos_;
    pos_ = 0;

    size_t space = size_ - valid_;
    if (space > 0) {
	size_t cc = tc_->read(&buf_[valid_], space);
	valid_ += cc;
	return cc;
    }

    return 0;
}
