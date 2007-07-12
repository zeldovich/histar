#ifndef LINUX_ARCH_INCLUDE_LINUXTHREAD_H
#define LINUX_ARCH_INCLUDE_LINUXTHREAD_H

void  linux_thread_run(int (*threadfn)(void *data), void *data,  const char *name);
void  linux_thread_flushsig(void);

#endif
