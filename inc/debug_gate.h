#ifndef _JOS_INC_DEBUG_GATE_H
#define _JOS_INC_DEBUG_GATE_H

struct debug_args 
{
    
};

int debug_gate_send(struct cobj_ref gate, struct debug_args *da);
void debug_gate_init(void);
void debug_gate_close(void);

#endif
