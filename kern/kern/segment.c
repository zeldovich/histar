#include <machine/pmap.h>
#include <kern/label.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <kern/kobj.h>
#include <inc/error.h>

int
segment_alloc(const struct Label *l, struct Segment **sgp)
{
    struct kobject *ko;
    int r = kobject_alloc(kobj_segment, l, &ko);
    if (r < 0)
	return r;

    struct Segment *sg = &ko->sg;
    segment_swapin(sg);

    *sgp = sg;
    return 0;
}

void
segment_swapin(struct Segment *sg)
{
    LIST_INIT(&sg->sg_segmap_list);
}

int
segment_copy(const struct Segment *src, const struct Label *newl,
	     struct Segment **dstp)
{
    struct Segment *dst;
    int r = segment_alloc(newl, &dst);
    if (r < 0)
	return r;

    segment_invalidate(src);
    r = kobject_copy_pages(&src->sg_ko, &dst->sg_ko);
    if (r < 0)
	return r;

    *dstp = dst;
    return 0;
}

int
segment_set_nbytes(struct Segment *sg, uint64_t num_bytes, uint8_t final)
{
    if (sg->sg_fixed_size)
	return -E_FIXEDSIZE;

    if (sg->sg_ko.ko_nbytes != num_bytes) {
	if (sg->sg_ko.ko_nbytes > num_bytes) {
	    segment_invalidate(sg);
	    if (sg->sg_ko.ko_pin_pg)
		return -E_BUSY;
	}

	int r = kobject_set_nbytes(&sg->sg_ko, num_bytes);
	if (r < 0)
	    return r;
    }

    sg->sg_fixed_size = final;
    return 0;
}

void
segment_map_ro(struct Segment *sg)
{
    struct segment_mapping *sm;
    LIST_FOREACH(sm, &sg->sg_segmap_list, sm_link)
	as_map_ro_sm(sm);
}

void
segment_invalidate(const struct Segment *sg)
{
    while (!LIST_EMPTY(&sg->sg_segmap_list)) {
	struct segment_mapping *sm = LIST_FIRST(&sg->sg_segmap_list);
	as_invalidate_sm(sm);
    }
}

void
segment_collect_dirty(const struct Segment *sg)
{
    struct segment_mapping *sm;
    LIST_FOREACH(sm, &sg->sg_segmap_list, sm_link)
	as_collect_dirty_sm(sm);
}
