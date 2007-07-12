#ifndef _ASM_LIND_TIMEX_H
#define _ASM_LIND_TIMEX_H

#include <asm/processor.h>

#define CLOCK_TICK_RATE	(HZ)

typedef unsigned long long cycles_t;

static inline cycles_t get_cycles (void)
{
	unsigned long long ret;

	rdtscll(ret);
	return ret;
}

#endif
