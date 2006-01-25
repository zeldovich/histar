#ifndef JOS_INC_STACK_H
#define JOS_INC_STACK_H

void stack_switch(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4,
		  void *stacktop, void *fn) __attribute__((noreturn));

#endif
