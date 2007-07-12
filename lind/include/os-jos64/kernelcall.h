#ifndef LINUX_ARCH_INCLUDE_OS_JOS64_KERNELCALL_H
#define LINUX_ARCH_INCLUDE_OS_JOS64_KERNELCALL_H

int kernel_call(void (*fn)(uint64_t a, uint64_t b), uint64_t a, uint64_t b);
int kernel_call_init(void) 
     __attribute__((__noreturn__));

#endif
