#ifndef JOS_INC_SEGMENT_H
#define JOS_INC_SEGMENT_H

#include <inc/types.h>
#include <inc/container.h>

/*
 * Must define at least one of these for the entry to be valid.
 * These match with the ELF flags (inc/elf64.h).
 */
#define SEGMAP_EXEC		0x01
#define SEGMAP_WRITE		0x02
#define SEGMAP_READ		0x04

/*
 * User-interpreted flags
 */
#define SEGMAP_CLOEXEC		0x0008
#define SEGMAP_DELAYED_UNMAP	0x0010
#define SEGMAP_ANON_MMAP	0x0020
#define SEGMAP_VECTOR_PF	0x0040
#define SEGMAP_STACK		0x0080
#define SEGMAP_RESERVE		0x0100

struct u_segment_mapping {
    struct cobj_ref segment;
    uint64_t start_page;
    uint64_t num_pages;
    uint32_t kslot;
    uint32_t flags;
    void *va;
};

struct u_address_space {
    void *trap_handler;
    void *trap_stack_base;
    void *trap_stack_top;
    uint64_t size;
    uint64_t nent;
    struct u_segment_mapping *ents;
};

#endif
