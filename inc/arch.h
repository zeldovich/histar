#ifndef JOS_INC_ARCH_H
#define JOS_INC_ARCH_H

uint64_t arch_read_tsc(void);

int arch_utrap_is_masked(void);
int arch_utrap_set_mask(int masked);

#endif
