#ifndef LINUX_ARCH_INCLUDE_ARCHTYPE_H
#define LINUX_ARCH_INCLUDE_ARCHTYPE_H

typedef enum {
    SIGNAL_ALARM = 0x1,
    SIGNAL_ETH   = 0x2,
    SIGNAL_NETD  = 0x4,
    SIGNAL_KCALL = 0x8
} signal_t;

typedef void (signal_handler_t)(signal_t);

#endif
