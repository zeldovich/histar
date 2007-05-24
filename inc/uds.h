#ifndef JOS_INC_UDS_H
#define JOS_INC_UDS_H

int uds_socket(int domain, int type, int protocol);

struct uds_slot {
    volatile uint64_t op;
    struct cobj_ref bipipe_seg;
    uint64_t taint;
    uint64_t grant;
    
    struct cobj_ref priv_gt;
};

#endif
