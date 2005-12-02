#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <machine/pmap.h>
#include <kern/kobj.h>
#include <inc/segment.h>

struct segment_header {
    struct kobject ko;

    uint64_t num_pages;
};

#define NUM_SG_PAGES	((PGSIZE - sizeof(struct segment_header)) / 8)
struct Segment {
    struct segment_header sg_hdr;
    void *sg_page[NUM_SG_PAGES];
};

int  segment_alloc(struct Label *l, struct Segment **sgp);
void segment_gc(struct Segment *sg);

int  segment_set_npages(struct Segment *sg, uint64_t num_pages);
int  segment_map_to_pmap(struct segment_map *segmap, struct Pagemap *pgmap);

#endif
