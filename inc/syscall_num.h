#ifndef JOS_INC_SYSCALLNUM_H
#define JOS_INC_SYSCALLNUM_H

typedef enum {
    SYS_cputs = 0,
    SYS_yield,
    SYS_halt,
    NSYSCALLS
} syscall_num;

#endif
