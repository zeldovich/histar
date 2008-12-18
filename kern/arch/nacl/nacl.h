#ifndef JOS_MACHINE_NACL_H
#define JOS_MACHINE_NACL_H

#include <machine/types.h>

void nacl_mem_init(const char *memfn, const char *binfn);
void nacl_trap_init(void);
void nacl_timer_init(void);

int  nacl_mmap(void *va, void *pp, int len, int prot);

#endif
