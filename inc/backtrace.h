#ifndef JOS_INC_BACKTRACE_H
#define JOS_INC_BACKTRACE_H

#include <machine/utrap.h>

int  backtrace(void **tracebuf, int maxents);
int  backtrace_utf(void **tracebuf, int maxents, const struct UTrapframe *utf);

#endif
