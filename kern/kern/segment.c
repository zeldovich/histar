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

    r = segment_set_npages(dst, src->sg_ko.ko_npages);
    if (r < 0)
	return r;

    for (uint64_t i = 0; i < src->sg_ko.ko_npages; i++) {
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
segment_set_npages(struct Segment *sg, uint64_t num_pages)
{
    return kobject_set_npages(&sg->sg_ko, num_pages);
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
