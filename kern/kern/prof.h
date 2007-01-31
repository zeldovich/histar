#ifndef PROF_H_
#define PROF_H_

#include <machine/types.h>
#include <kern/syscall.h>

void prof_init(void);
void prof_syscall(uint64_t num, uint64_t time);
void prof_trap(uint64_t num, uint64_t time);
void prof_user(uint64_t time);
void prof_thread(uint64_t tid, uint64_t time);
void prof_print(void);

void __cyg_profile_func_enter(void *this_fn, void *call_site);
void __cyg_profile_func_exit(void *this_fn, void *call_site);
void cyg_profile_print(void);
void cyg_profile_free_stack(uint64_t sp);

#endif /*PROF_H_ */
