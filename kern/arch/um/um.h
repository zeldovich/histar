#ifndef JOS_MACHINE_UM_H
#define JOS_MACHINE_UM_H

#include <machine/types.h>

extern uint64_t um_mem_bytes;
extern void *um_mem_base;

void um_mem_init(uint64_t bytes);
void um_cons_init(void);
void um_time_init(void);

#endif
