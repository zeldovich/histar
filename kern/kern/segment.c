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
segment_copy(const struct Segment *src, struct Label *newl,
	     struct Segment **dstp)
{
    struct Segment *dst;
    int r = segment_alloc(newl, &dst);
    if (r < 0)
	return r;

    r = segment_set_nbytes(dst, src->sg_ko.ko_nbytes);
    if (r < 0)
	return r;

    for (uint64_t i = 0; i < kobject_npages(&src->sg_ko); i++) {
	void *srcpg, *dstpg;
	r = kobject_get_page(&src->sg_ko, i, &srcpg, page_ro);
	if (r < 0)
	    return r;
	r = kobject_get_page(&dst->sg_ko, i, &dstpg, page_rw);
	if (r < 0)
	    return r;

	memcpy(dstpg, srcpg, PGSIZE);
    }

    *dstp = dst;
    return 0;
}

int
segment_set_nbytes(struct Segment *sg, uint64_t num_bytes)
{
    segment_invalidate(sg);
    return kobject_set_nbytes(&sg->sg_ko, num_bytes);
}

void
segment_snapshot(struct Segment *sg)
{
    segment_invalidate(sg);
}

void
segment_invalidate(struct Segment *sg)
{
    while (!LIST_EMPTY(&sg->sg_segmap_list)) {
	struct segment_mapping *sm = LIST_FIRST(&sg->sg_segmap_list);
	as_invalidate(sm->sm_as);
    }
}
