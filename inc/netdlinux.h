#ifndef JOS_INC_NETDLINUX_H
#define JOS_INC_NETDLINUX_H

#include <inc/jcomm.h>

typedef int (*netd_jcomm_handler)(struct jcomm_ref);

int netd_linux_server_init(netd_jcomm_handler handler);

#endif
