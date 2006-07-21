#ifndef _JOS_INC_DEBUG_GATE_H
#define _JOS_INC_DEBUG_GATE_H

struct sigcontext;

typedef enum {
    da_wait = 1,
    da_cont,
    da_getregs,
    da_getfpregs,
    da_peektext,
    da_poketext,
    da_setregs,
    da_setfpregs,
} debug_args_op_t;

struct debug_args 
{
    debug_args_op_t op;
    uint64_t addr;
    uint64_t word;
    struct cobj_ref arg_cobj;
    // return
    int64_t ret;
    uint64_t ret_gen;
    uint64_t ret_word;
    struct cobj_ref ret_cobj;
};

// from <sys/user.h>
typedef struct user_regs_struct debug_regs;
typedef struct user_fpregs_struct debug_fpregs;

int64_t debug_gate_send(struct cobj_ref gate, struct debug_args *da);

// to be used locally
void debug_gate_init(void);
void debug_gate_reset(void);
void debug_gate_close(void);
void debug_gate_signal_stop(char signo, struct sigcontext *sc);


#endif
