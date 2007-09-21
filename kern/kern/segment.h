#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <kern/as.h>
#include <kern/kobjhdr.h>
#include <inc/segment.h>

struct Segment {
    struct kobject_hdr sg_ko;
    struct segmap_list sg_segmap_list;
};

int  segment_alloc(const struct Label *l, struct Segment **sgp)
    __attribute__ ((warn_unused_result));
int  segment_copy(const struct Segment *src, const struct Label *newl,
		  struct Segment **dstp)
    __attribute__ ((warn_unused_result));
int  segment_set_nbytes(struct Segment *sg, uint64_t num_bytes)
    __attribute__ ((warn_unused_result));
void segment_map_ro(struct Segment *sg);
void segment_invalidate(const struct Segment *sg, uint64_t parent_ct);
void segment_collect_dirty(const struct Segment *sg);
void segment_swapin(struct Segment *sg);
void segment_on_decref(struct Segment *sg, uint64_t parent_ct);

#endif
