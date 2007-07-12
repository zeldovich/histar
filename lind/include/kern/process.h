#ifndef LINUX_ARCH_INCLUDE_KERN_PROCESS_H
#define LINUX_ARCH_INCLUDE_KERN_PROCESS_H

void start_idle_thread(void *stack, jmp_buf *switch_buf);

#endif
