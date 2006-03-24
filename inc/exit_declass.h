#ifndef JOS_INC_EXIT_DECLASS_H
#define JOS_INC_EXIT_DECLASS_H

#include <inc/container.h>

struct exit_declass_args {
    struct cobj_ref status_seg;
    uint64_t parent_pid;
};

void exit_declassifier(void *, struct gate_call_data *, struct gatesrv_return *);

#endif
