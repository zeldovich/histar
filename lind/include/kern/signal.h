#ifndef LINUX_ARCH_INCLUDE_KERN_SIGNAL_H
#define LINUX_ARCH_INCLUDE_KERN_SIGNAL_H

int get_signals(void);
int set_signals(int enable);
void lind_signal_init(void);

#endif
