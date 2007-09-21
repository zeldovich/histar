#ifndef LINUX_ARCH_INCLUDE_TRANSPORT_H
#define LINUX_ARCH_INCLUDE_TRANSPORT_H

#include <linux/if_ether.h>

struct transport {
    spinlock_t lock;
    char name[16];
    unsigned char mac[ETH_ALEN];
    unsigned short mtu;

    int (*open)(const char *name, void *data);
    int (*close)(void *data);
    int (*tx)(void *data, void *buf, unsigned int buf_len);
    int (*rx)(void *data, void *buf, unsigned int buf_len);
    void (*irq_reset)(void *data);

    unsigned int data_len;
    char data[0];
};

int register_transport(struct transport *trans);

#endif
