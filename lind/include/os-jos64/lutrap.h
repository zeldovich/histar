#ifndef LINUX_ARCH_INCLUDE_OS_JOS64_LUTRAP_H
#define LINUX_ARCH_INCLUDE_OS_JOS64_LUTRAP_H

#include <archtype.h>

void lutrap_signal(signal_handler_t *h);
void lutrap_kill(signal_t sig);
int lutrap_init(void);

#endif
