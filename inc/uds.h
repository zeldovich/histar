#ifndef JOS_INC_UDS_H
#define JOS_INC_UDS_H

#include <inc/jcomm.h>

int uds_socket(int domain, int type, int protocol);

struct uds_slot {
    volatile uint64_t op;
    struct jcomm jc;
    uint64_t taint;
    uint64_t grant;
    
    struct cobj_ref priv_gt;
};

#endif
