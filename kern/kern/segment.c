#include <machine/pmap.h>
#include <machine/thread.h>
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

    *sgp = sg;
    return 0;
}

int
segment_set_npages(struct Segment *sg, uint64_t num_pages)
{
    return kobject_set_npages(&sg->sg_ko, num_pages);
}

static int
segment_map(struct Pagemap *pgmap, struct Segment *sg, void *va,
	    uint64_t start_page, uint64_t num_pages, bool_t writable)
{
    char *cva = (char *) va;
    if (PGOFF(cva))
	return -E_INVAL;

    for (int64_t i = start_page; i < start_page + num_pages; i++) {
	void *pp;
	int r = kobject_get_page(&sg->sg_ko, i, &pp);

	if (((uint64_t) cva) >= ULIM)
	    r = -E_INVAL;
	if (r == 0)
	    r = page_insert(pgmap, pp, cva,
			    PTE_U | (writable ? PTE_W : 0));
	if (r < 0) {
	    // unmap pages
	    for (; i >= 0; i--) {
		page_remove(pgmap, cva);
		cva -= PGSIZE;
	    }
	    return r;
	}

	cva += PGSIZE;
    }

    return 0;
}

int
segment_map_fill_pmap(struct segment_map *segmap, struct Pagemap *pgmap, void *va)
{
    for (int i = 0; i < NUM_SG_MAPPINGS; i++) {
	if (segmap->sm_ent[i].num_pages == 0)
	    continue;

	void *va_start = segmap->sm_ent[i].va;
	void *va_end = va_start + segmap->sm_ent[i].num_pages * PGSIZE;
	if (va < va_start || va >= va_end)
	    continue;

	struct Segment *sg;
	int r = cobj_get(segmap->sm_ent[i].segment, kobj_segment, (struct kobject **)&sg);
	if (r < 0)
	    return r;

	r = label_compare(&sg->sg_ko.ko_label,
			  &cur_thread->th_ko.ko_label,
			  segmap->sm_ent[i].writable ? label_eq : label_leq_starhi);
	if (r < 0)
	    return r;

	r = segment_map(pgmap, sg,
			segmap->sm_ent[i].va,
			segmap->sm_ent[i].start_page,
			segmap->sm_ent[i].num_pages,
			segmap->sm_ent[i].writable);
	return r;
    }

    return -E_FAULT;
}
