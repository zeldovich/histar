#ifndef JOS_INC_SEGMENT_H
#define JOS_INC_SEGMENT_H

#include <inc/types.h>
#include <inc/container.h>

struct segment_mapping {
    struct cobj_ref segment;
    uint64_t start_page;
    uint64_t num_pages;
    bool_t writable;
    void *va;
};

#define NUM_SG_MAPPINGS 16
struct segment_map {
    struct segment_mapping sm_ent[NUM_SG_MAPPINGS];
};

#endif
