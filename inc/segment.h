#ifndef JOS_INC_SEGMENT_H
#define JOS_INC_SEGMENT_H

#include <inc/types.h>
#include <inc/container.h>

// Must define at least one of these for the entry to be valid.
// These match with the ELF flags (inc/elf64.h).
#define SEGMAP_EXEC	0x01
#define SEGMAP_WRITE	0x02
#define SEGMAP_READ	0x04

struct segment_mapping {
    struct cobj_ref segment;
    uint64_t start_page;
    uint64_t num_pages;
    uint64_t flags;
    void *va;
};

struct u_address_space {
    uint64_t size;
    uint64_t nent;
    struct segment_mapping *ents;
};

#endif
