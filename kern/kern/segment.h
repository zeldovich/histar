#ifndef JOS_KERN_SEGMENT_H
#define JOS_KERN_SEGMENT_H

#include <machine/types.h>
#include <machine/as.h>
#include <kern/kobjhdr.h>
#include <inc/segment.h>

struct Segment {
    struct kobject_hdr sg_ko;
    struct segmap_list sg_segmap_list;
    uint8_t sg_fixed_size;
};

int  segment_alloc(const struct Label *l, struct Segment **sgp)
    __attribute__ ((warn_unused_result));
int  segment_copy(const struct Segment *src, const struct Label *newl,
		  struct Segment **dstp)
    __attribute__ ((warn_unused_result));
int  segment_set_nbytes(struct Segment *sg, uint64_t num_bytes, uint8_t final)
    __attribute__ ((warn_unused_result));
void segment_snapshot(struct Segment *sg);
void segment_invalidate(const struct Segment *sg);
void segment_collect_dirty(const struct Segment *sg);
void segment_swapin(struct Segment *sg);

#endif
