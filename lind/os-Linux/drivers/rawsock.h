#ifndef LINUX_ARCH_LIND_RAWSOCK_H
#define LINUX_ARCH_LIND_RAWSOCK_H

struct rawsock_data {
    int sockfd;
};

int rawsock_open(const char *name, void *data);
int rawsock_tx(void *data, void *buf, unsigned int buf_len);
int rawsock_rx(void *data, void *buf, unsigned int buf_len);
int rawsock_mac(unsigned char *buf);

#endif
