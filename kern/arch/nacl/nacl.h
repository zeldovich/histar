#ifndef JOS_MACHINE_NACL_H
#define JOS_MACHINE_NACL_H

#include <machine/types.h>

void nacl_mem_init(const char *memfn, const char *binfn);
void nacl_trap_init(void);
void nacl_timer_init(void);
void nacl_seg_init(void);

int  nacl_mmap(void *va, void *pp, int len, int prot);

/* Setup by nacl_seg_init */
extern uint16_t user_cs, user_ds;
extern uint16_t kern_cs, kern_ds;

#endif
