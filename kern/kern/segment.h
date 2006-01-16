#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <machine/as.h>
#include <kern/kobj.h>
#include <inc/segment.h>

struct Segment {
    struct kobject sg_ko;
    struct segmap_list sg_segmap_list;
};

int  segment_alloc(struct Label *l, struct Segment **sgp);
int  segment_copy(struct Segment *src, struct Label *newl,
		  struct Segment **dstp);
int  segment_set_npages(struct Segment *sg, uint64_t num_pages);

#endif
