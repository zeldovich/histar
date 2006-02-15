extern "C" {
#include <inc/lib.h>
#include <inc/netd.h>
#include <inc/string.h>
#include <inc/fs.h>
}

static int fetch_debug = 1;

class url {
public:	
    url(const char *s) : host_(0), path_(0) {
	static const char *prefix = "http://";
	if (strncmp(s, prefix, strlen(prefix)))
	    panic("bad URL type: %s", s);

	const char *h = s + strlen(prefix);
	char *slash = strchr(h, '/');
	if (slash == 0)
	    panic("poorly formatted URL: %s", s);

	size_t hostlen = slash - h;
	host_ = (char *) malloc(hostlen + 1);
	path_ = (char *) malloc(strlen(slash) + 1);

	if (!host_ || !path_)
	    panic("out of memory");

	strncpy(host_, h, hostlen);
	host_[hostlen] = '\0';
	strcpy(path_, slash);
    }

    ~url() {
	if (host_)
	    free(host_);
	if (path_)
	    free(path_);
    }

    const char *host() {
	return host_;
    }

    const char *path() {
	return path_;
    }

private:
    char *host_;
    char *path_;
};

class tcpconn {
public:
    tcpconn(const char *hostname, uint16_t port) : fd_(-1) {
	uint32_t ip = 0;
	const char *p = hostname;

	for (int i = 0; i < 4; i++) {
	    if (p == 0)
		panic("bad ip address: %s", hostname);

	    int v = atoi(p);
	    p = strchr(p, '.');
	    if (p != 0)
		p++;

	    if (v < 0 || v >= 256)
		panic("bad ip octet: %s", hostname);

	    ip = (ip << 8) | v;
	}

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(ip);

	fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_ < 0)
	    panic("socket: %s", e2s(fd_));

	int r = connect(fd_, (struct sockaddr *) &sin, sizeof(sin));
	if (r < 0)
	    panic("connect: %s", e2s(r));
    }

    ~tcpconn() {
	if (fd_ >= 0)
	    close(fd_);
    }

    void write(const char *buf, size_t count) {
	size_t done = 0;

	while (done < count) {
	    ssize_t r = ::write(fd_, buf + done, count - done);
	    if (r <= 0)
		panic("cannot write: %s", e2s(r));
	    done += r;
	}
    }

    size_t read(char *buf, size_t len) {
	ssize_t r = ::read(fd_, buf, len);
	if (r < 0)
	    panic("cannot read: %s", e2s(r));

	return r;
    }

private:
    int fd_;
};

class lineparser {
public:
    lineparser(tcpconn *tc) : tc_(tc) {
	pos_ = 0;
	size_ = 4096;
	valid_ = 0;
	buf_ = (char *) malloc(size_);
	if (buf_ == 0)
	    panic("out of memory");
    }

    ~lineparser() {
	if (buf_)
	    free(buf_);
    }

    size_t read(char *buf, size_t len) {
	if (valid_ == pos_)
	    refill();

	size_t queued = valid_ - pos_;
	if (queued > len)
	    queued = len;
	memcpy(buf, &buf_[pos_], queued);
	pos_ += queued;
	return queued;
    }

    const char *read_line() {
	refill();

	char *base = &buf_[pos_];
	char *newline = strstr(base, "\r\n");
	if (newline == 0)
	    return 0;

	*newline = '\0';
	pos_ += (newline - base) + 2;
	return base;
    }

private:
    void refill() {
	memmove(&buf_[0], &buf_[pos_], valid_ - pos_);
	valid_ -= pos_;
	pos_ = 0;

	size_t space = size_ - valid_;
	if (space > 0) {
	    size_t cc = tc_->read(&buf_[valid_], space);
	    valid_ += cc;

	    if (fetch_debug)
		printf("lineparser::refill: got another %ld bytes\n", cc);
	}
    }

    tcpconn *tc_;

    size_t size_;
    char *buf_;

    size_t pos_;
    size_t valid_;
};

int
main(int ac, char **av)
{
    if (ac != 3) {
	printf("Usage: %s url filename\n", av[0]);
	return -1;
    }

    const char *ustr = av[1];
    char *pn = av[2];

    printf("Fetching %s into %s\n", ustr, pn);

    url u(av[1]);
    if (fetch_debug)
	printf("URL: host %s path %s\n", u.host(), u.path());

    tcpconn tc(u.host(), 80);
    if (fetch_debug)
	printf("Connected OK\n");

    char buf[1024];
    sprintf(buf, "GET %s HTTP/1.0\r\n\r\n", u.path());
    tc.write(buf, strlen(buf));
    if (fetch_debug)
	printf("Sent request OK\n");

    lineparser lp(&tc);
    const char *resp = lp.read_line();
    assert(resp);
    if (strncmp(resp, "HTTP/1.", 7))
	panic("not an HTTP response: %s", resp);
    if (strncmp(&resp[9], "200", 3))
	panic("request error: %s", resp);

    while (resp[0] != '\0') {
	resp = lp.read_line();
	assert(resp);
    }

    const char *dirname, *basename;
    fs_dirbase(pn, &dirname, &basename);

    struct fs_inode dir;
    int r = fs_namei(dirname, &dir);
    if (r < 0)
	panic("cannot find directory %s: %s", dirname, e2s(r));

    struct fs_inode file;
    r = fs_create(dir, basename, &file);
    if (r < 0)
	panic("cannot create file %s: %s", basename, e2s(r));

    uint64_t off = 0;
    for (;;) {
	size_t cc = lp.read(buf, sizeof(buf));
	if (cc == 0)
	    break;

	r = fs_pwrite(file, buf, cc, off);
	if (r < 0)
	    panic("fs_pwrite: %s", e2s(r));

	off += cc;

	if (fetch_debug)
	    printf("wrote %ld bytes to file\n", cc);
    }

    printf("Done.\n");
}
