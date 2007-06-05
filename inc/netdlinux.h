#ifndef JOS_INC_NETD_LINUX_H
#define JOS_INC_NETD_LINUX_H

int netd_linux_server_init(void (*handler)(struct netd_op_args *));

#endif
