#ifndef JOS_INC_COOPERATE_H
#define JOS_INC_COOPERATE_H

#define COOP_STATUS	0x1000
#define COOP_RETVAL	0x1008
#define COOP_TEXT	0x2000

#ifndef __ASSEMBLER__

/*
 * This is mapped at COOP_STATUS.
 */
struct coop_status {
    uint64_t done;
    int64_t rval;
};

/*
 * This specifies where to get system call argument values,
 * and its address is passed in through %rdi.  It's actually
 * right after the code text at COOP_TEXT.
 */
struct coop_syscall_args {
    uint64_t *args[8];
};

struct coop_syscall_argval {
    uint64_t argval[8];
};

#endif

#endif
