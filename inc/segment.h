#ifndef JOS_INC_SEGMENT_H
#define JOS_INC_SEGMENT_H

#include <inc/types.h>
#include <inc/container.h>

typedef enum {
    segment_map_ro = 0,
    segment_map_rw,
    segment_map_cow
} segment_map_mode;

struct segment_map_args {
    struct cobj_ref segment;
    struct cobj_ref as;
    void *va;
    uint64_t start_page;
    uint64_t num_pages;
    segment_map_mode mode;
};

#endif
