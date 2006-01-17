#include <machine/pmap.h>
#include <kern/label.h>
#include <kern/container.h>
#include <kern/segment.h>
#include <inc/error.h>

int
segment_alloc(struct Label *l, struct Segment **sgp)
{
    struct Segment *sg;
    int r = kobject_alloc(kobj_segment, l, (struct kobject **)&sg);
    if (r < 0)
	return r;

    static_assert(sizeof(*sg) <= sizeof(struct kobject_buf));
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
segment_copy(struct Segment *src, struct Label *newl, struct Segment **dstp)
{
    struct Segment *dst;
    int r = segment_alloc(newl, &dst);
    if (r < 0)
	return r;

    r = segment_set_npages(dst, src->sg_ko.ko_npages);
    if (r < 0)
	return r;

    for (int i = 0; i < src->sg_ko.ko_npages; i++) {
	void *srcpg, *dstpg;
	r = kobject_get_page(&src->sg_ko, i, &srcpg, kobj_ro);
	if (r < 0)
	    return r;
	r = kobject_get_page(&dst->sg_ko, i, &dstpg, kobj_rw);
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
    struct segment_mapping *sm;
    LIST_FOREACH(sm, &sg->sg_segmap_list, sm_link)
	as_segmap_snapshot(sm->sm_as, sm);
}
