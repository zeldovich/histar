#ifndef JOS_INC_NETHELPER_HH
#define JOS_INC_NETHELPER_HH

extern "C" {
#include <inc/types.h>
}

class url {
public:	
    url(const char *s);
    ~url();

    const char *host() { return host_; }
    const char *path() { return path_; }

private:
    char *host_;
    char *path_;
};

class tcpconn {
public:
    tcpconn(const char *hostname, uint16_t port);
    tcpconn(int fd);
    ~tcpconn();

    void write(const char *buf, size_t count);
    size_t read(char *buf, size_t len);

private:
    int fd_;
};

class lineparser {
public:
    lineparser(tcpconn *tc);
    ~lineparser();

    size_t read(char *buf, size_t len);
    const char *read_line();

private:
    size_t refill();

    tcpconn *tc_;

    size_t size_;
    char *buf_;

    size_t pos_;
    size_t valid_;
};

#endif
