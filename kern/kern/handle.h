#ifndef JOS_KERN_UNIQUE_H
#define JOS_KERN_UNIQUE_H

#include <machine/types.h>

extern uint64_t handle_counter;

uint64_t handle_alloc();

#endif
