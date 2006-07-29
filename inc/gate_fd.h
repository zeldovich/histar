#ifndef JOS_INC_GATE_FD_H
#define JOS_INC_GATE_FD_H

int gatefd(struct cobj_ref gate, int flags);

#define GATEFD_MAGIC 0x00001111000011110000

struct gatefd_args 
{
    uint64_t gfd_magic;
    struct cobj_ref gfd_seg;
    int gfd_ret;
} ;

#endif
