#ifndef JOS_INC_COOPERATE_H
#define JOS_INC_COOPERATE_H

#define COOP_STATUS	0x1000
#define COOP_RETVAL	0x1008
#define COOP_TEXT	0x2000
#define COOP_ARGS	0x3000

#ifndef __ASSEMBLER__

// This is mapped at COOP_STATUS
struct coop_status {
    uint64_t done;
    int64_t rval;
};

// This is mapped at COOP_ARGS
struct coop_syscall_args {
    uint64_t *args[8];
};

#endif

#endif
