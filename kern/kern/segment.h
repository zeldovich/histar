#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <kern/kobj.h>
#include <inc/segment.h>

struct Segment {
    struct kobject sg_ko;
};

int  segment_alloc(struct Label *l, struct Segment **sgp);
int  segment_set_npages(struct Segment *sg, uint64_t num_pages);
int  segment_map_fill_pmap(struct segment_map *segmap, struct Pagemap *pgmap, void *va);

#endif
