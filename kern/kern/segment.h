#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <inc/segment.h>
#include <machine/types.h>
#include <machine/mmu.h>
#include <machine/pmap.h>

struct segment_header {
    uint64_t num_pages;
    struct Label *label;
};

#define NUM_SG_PAGES	((PGSIZE - sizeof(struct segment_header)) / 8)
struct Segment {
    struct segment_header sg_hdr;
    struct Page *sg_page[NUM_SG_PAGES];
};

#define NUM_SG_MAPPINGS 16
struct segment_map {
    struct segment_mapping sm_ent[NUM_SG_MAPPINGS];
};

int  segment_alloc(struct Segment **sgp);
void segment_free(struct Segment *sg);
void segment_addref(struct Segment *sg);
void segment_decref(struct Segment *sg);
int  segment_set_npages(struct Segment *sg, uint64_t num_pages);

int  segment_map(struct Pagemap *pgmap, struct Segment *sg, void *va_start,
		 uint64_t start_page, uint64_t num_pages, segment_map_mode mode);

#endif
