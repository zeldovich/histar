#ifndef JOS_KERN_HANDLE_H
#define JOS_KERN_HANDLE_H

#include <machine/types.h>
#include <inc/bf60.h>

#define SYSTEM_KEY_SIZE		64
extern uint64_t handle_counter;
extern uint8_t system_key[SYSTEM_KEY_SIZE];
extern struct bf_ctx pstate_key_ctx;

void     key_generate(void);
void	 key_derive(void);

uint64_t handle_alloc(void);

#endif
