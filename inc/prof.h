#ifndef JOS_INC_PROF_H
#define JOS_INC_PROF_H

void prof_init(char on);
void prof_func(uint64_t time);
void prof_data(void *func_addr, uint64_t time);
void prof_print(char use_cprintf);

#endif
