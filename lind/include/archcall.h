#ifndef LINUX_ARCH_INCLUDE_ARCHCALL_H
#define LINUX_ARCH_INCLUDE_ARCHCALL_H

#include <asm-lind/setup.h>
#include <archtype.h>

void      arch_init(void);
long long arch_nsec(void);
void      arch_sleep(unsigned long secs);
void      arch_write_stderr(const char *string, unsigned len);
int       arch_run_kernel_thread(int (*fn)(void *), void *arg, void **jmp_ptr);
int       arch_printf(const char *format, ...);
void      arch_signal_handler(signal_handler_t *h);
void      arch_halt(int exit_code);
int       arch_exec(void);

int       arch_file_size(const char *pn, unsigned long *size);
int       arch_file_read(const char *pn, void *buf, unsigned long bytes);

#endif
