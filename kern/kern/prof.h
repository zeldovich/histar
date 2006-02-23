#ifndef PROF_H_
#define PROF_H_

#include <inc/types.h>
#include <inc/syscall_num.h>

#define PROF_PRINT 0

void prof_init(void) ;
void prof_syscall(syscall_num num, uint64_t time) ;
void prof_trap(int num, uint64_t time) ;
void prof_print(void) ;

#endif /*PROF_H_*/
