#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <machine/mmu.h>
#include <machine/pmap.h>
#include <kern/kobj.h>
#include <inc/segment.h>

#define NUM_SG_PAGES	((PGSIZE - sizeof(struct kobject)) / 8)
struct Segment {
    struct kobject sg_ko;
    void *sg_page[NUM_SG_PAGES];
};

int  segment_alloc(struct Label *l, struct Segment **sgp);
void segment_gc(struct Segment *sg);

int  segment_set_npages(struct Segment *sg, uint64_t num_pages);
int  segment_map_fill_pmap(struct segment_map *segmap, struct Pagemap *pgmap, void *va);

void segment_swapout(struct Segment *sg);
void segment_swapin_page(struct Segment *sg, uint64_t page_num, void *p);
void *segment_swapout_page(struct Segment *sg, uint64_t page_num);

#endif
