#ifndef JOS_KERN_HANDLE_H
#define JOS_KERN_HANDLE_H

#include <machine/types.h>

#define HANDLE_KEY_SIZE	64
extern uint64_t handle_counter;
extern uint8_t handle_key[HANDLE_KEY_SIZE];

uint64_t handle_alloc(void);
void     handle_key_generate(void);

#endif
