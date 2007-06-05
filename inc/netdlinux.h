#ifndef JOS_INC_NETDLINUX_H
#define JOS_INC_NETDLINUX_H

int netd_linux_server_init(void (*handler)(struct netd_op_args *));

#endif
