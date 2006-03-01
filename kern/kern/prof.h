#ifndef PROF_H_
#define PROF_H_

#include <machine/types.h>
#include <kern/syscall.h>

void prof_init(void) ;
void prof_syscall(syscall_num num, uint64_t time) ;
void prof_trap(int num, uint64_t time) ;
void prof_user(uint64_t time) ;
void prof_print(void) ;

void __cyg_profile_func_enter(void *this_fn, void *call_site) ;
void __cyg_profile_func_exit(void *this_fn, void *call_site) ;
void cyg_profile_print(void) ;


#endif /*PROF_H_*/
