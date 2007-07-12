#ifndef LINUX_ARCH_INCLUDE_OS_LIB_NETD_H
#define LINUX_ARCH_INCLUDE_OS_LIB_NETD_H

int netd_linux_init(void);
void netd_linux_main(void)
     __attribute__((__noreturn__));

#endif
